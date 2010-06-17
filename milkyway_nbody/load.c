/* ************************************************************************** */
/* LOAD.C: routines to create tree.  Public routines: maketree(). */
/* */
/* Copyright (c) 1993 by Joshua E. Barnes, Honolulu, HI. */
/* It's free because it's yours. */
/* ************************************************************************** */

#include "defs.h"
#include "code.h"

static void newtree(void);           /* flush existing tree */
static cellptr makecell(void);           /* create an empty cell */
static void expandbox(bodyptr, int);     /* set size of t.root cell */
static void loadbody(bodyptr);           /* load body into tree */
static int subindex(bodyptr, cellptr);       /* compute subcell index */
static void hackcofm(const NBodyCtx* ctx, cellptr, real);     /* find centers of mass */
static void setrcrit(const NBodyCtx*, cellptr, vector, real); /* set cell's crit. radius */
static void threadtree(nodeptr, nodeptr);    /* set next and more links */
static void hackquad(cellptr);           /* compute quad moments */

/* maketree: initialize tree structure for hierarchical force calculation
 * from body array btab, which contains ctx.nbody bodies.
 */
void maketree(const NBodyCtx* ctx, bodyptr btab, int nbody)
{
    bodyptr p;

    newtree();                                  /* flush existing tree, etc */
    t.root = makecell();              /* allocate the t.root cell */
    CLRV(Pos(t.root));                /* initialize the midpoint */
    expandbox(btab, nbody);                     /* and expand cell to fit */
    t.maxlevel = 0;                               /* init count of levels */
    for (p = btab; p < btab + nbody; p++)       /* loop over bodies... */
        if (Mass(p) != 0.0)                     /* exclude test particles */
            loadbody(p);                        /* and insert into tree */
    hackcofm(ctx, t.root, t.rsize);                  /* find c-of-m coordinates */
    threadtree((nodeptr) t.root, NULL);           /* add Next and More links */
    if (ctx->usequad)                /* including quad moments? */
        hackquad(t.root);                         /* assign Quad moments */
}

/* newtree: reclaim cells in tree, prepare to build new one. */
static nodeptr freecell = NULL;              /* list of free cells */

static void newtree(void)
{
    static bool firstcall = TRUE;
    nodeptr p;

    if (! firstcall)                            /* tree data to reclaim? */
    {
        p = (nodeptr) t.root;                     /* start with the t.root */
        while (p != NULL)                       /* loop scanning tree */
            if (Type(p) == CELL)                /* found cell to free? */
            {
                Next(p) = freecell;             /* link to front of */
                freecell = p;                   /* ...existing list */
                p = More(p);                    /* scan down tree */
            }
            else                                /* skip over bodies */
                p = Next(p);                    /* go on to next */
    }
    else                                        /* first time through */
        firstcall = FALSE;                      /* so just note it */
    t.root = NULL;                                /* flush existing tree */
    t.cellused = 0;                               /* reset cell count */
}

/*  * MAKECELL: return pointer to free cell.
 */

static cellptr makecell(void)
{
    cellptr c;
    int i;

    if (freecell == NULL)                       /* no free cells left? */
        c = (cellptr) allocate(sizeof(cell));   /* allocate a new one */
    else                                        /* use existing free cell */
    {
        c = (cellptr) freecell;                 /* take one on front */
        freecell = Next(c);                     /* go on to next one */
    }
    Type(c) = CELL;                             /* initialize cell type */
    for (i = 0; i < NSUB; i++)                  /* loop over subcells */
        Subp(c)[i] = NULL;                      /* and empty each one */
    t.cellused++;                                 /* count one more cell */
    return (c);
}

/* EXPANDBOX: find range of coordinate values (with respect to t.root)
 * and expand t.root cell to fit.  The size is doubled at each step to
 * take advantage of exact representation of powers of two.
 */

static void expandbox(bodyptr btab, int nbody)
{
    real xyzmax;
    bodyptr p;
    int k;

    xyzmax = 0.0;
    for (p = btab; p < btab + nbody; p++)
        for (k = 0; k < NDIM; k++)
            xyzmax = MAX(xyzmax, rabs(Pos(p)[k] - Pos(t.root)[k]));

    while (t.rsize < 2 * xyzmax)
    {
        t.rsize = 2 * t.rsize;
    }
}

/* LOADBODY: descend tree and insert body p in appropriate place. */

static void loadbody(bodyptr p)
{
    cellptr q, c;
    int qind, lev, k;
    real qsize;

    q = t.root;                                   /* start with tree t.root */
    qind = subindex(p, q);          /* get index of subcell */
    qsize = t.rsize;                              /* keep track of cell size */
    lev = 0;                                    /* count levels descended */
    while (Subp(q)[qind] != NULL)               /* loop descending tree */
    {
        if (Type(Subp(q)[qind]) == BODY)        /* reached a "leaf"? */
        {
            c = makecell();                     /* allocate new cell */
            for (k = 0; k < NDIM; k++)      /* initialize midpoint */
                Pos(c)[k] = Pos(q)[k] +     /* offset from parent */
                            (Pos(p)[k] < Pos(q)[k] ? - qsize : qsize) / 4;
            Subp(c)[subindex((bodyptr) Subp(q)[qind], c)] = Subp(q)[qind];
            /* put body in cell */
            Subp(q)[qind] = (nodeptr) c;        /* link cell in tree */
        }
        q = (cellptr) Subp(q)[qind];        /* advance to next level */
        qind = subindex(p, q);          /* get index to examine */
        qsize = qsize / 2;                      /* shrink current cell */
        lev++;                                  /* count another level */
    }
    Subp(q)[qind] = (nodeptr) p;                /* found place, store p */
    t.maxlevel = MAX(t.maxlevel, lev);      /* remember maximum level */
}

/*  * SUBINDEX: compute subcell index for body p in cell q.
 */

static int subindex(bodyptr p, cellptr q)
{
    int ind, k;

    ind = 0;                    /* accumulate subcell index */
    for (k = 0; k < NDIM; k++)          /* loop over dimensions */
        if (Pos(q)[k] <= Pos(p)[k])     /* if beyond midpoint */
            ind += NSUB >> (k + 1);             /* skip over subcells */
    return (ind);
}

/* hackcofm: descend tree finding center-of-mass coordinates and
 * setting critical cell radii.
 */

static void hackcofm(const NBodyCtx* ctx, cellptr p, real psize)
{
    vector cmpos, tmpv;
    int i, k;
    nodeptr q;

    Mass(p) = 0.0;                              /* init total mass... */
    CLRV(cmpos);                                /* and center of mass */
    for (i = 0; i < NSUB; i++)                  /* loop over subnodes */
        if ((q = Subp(p)[i]) != NULL)           /* does subnode exist? */
        {
            if (Type(q) == CELL)        /* and is it a cell? */
                hackcofm(ctx, (cellptr) q, psize / 2); /* find subcell cm */
            Mass(p) += Mass(q);                 /* sum total mass */
            MULVS(tmpv, Pos(q), Mass(q));       /* weight pos by mass */
            ADDV(cmpos, cmpos, tmpv);           /* sum c-of-m position */
        }
    DIVVS(cmpos, cmpos, Mass(p));               /* rescale cms position */
    for (k = 0; k < NDIM; k++)          /* check tree structure... */
        if (cmpos[k] < Pos(p)[k] - psize / 2 || /* if out of bounds */
                Pos(p)[k] + psize / 2 <= cmpos[k]) /* in either direction */
            error("hackcofm: tree structure error\n");
    setrcrit(ctx, p, cmpos, psize);                  /* set critical radius */
    SETV(Pos(p), cmpos);            /* and center-of-mass pos */
}

/* setrcrit: assign critical radius for cell p, using center-of-mass
 * position cmpos and cell size psize. */
static void setrcrit(const NBodyCtx* ctx, cellptr p, vector cmpos, real psize)
{
    real rc, bmax2, dmin;
    int k;

    if (ctx->theta == 0.0)               /* exact force calculation? */
        rc = 2 * t.rsize;                /* always open cells */
    else if (ctx->criterion == BH86)     /* use old BH criterion? */
        rc = psize / ctx->theta;         /* using size of cell */
    else if (ctx->criterion == SW93)     /* use S&W's criterion? */
    {
        bmax2 = 0.0;                     /* compute max distance^2 */
        for (k = 0; k < NDIM; k++)       /* loop over dimensions */
        {
            dmin = cmpos[k] - (Pos(p)[k] - psize / 2);
            /* dist from 1st corner */
            bmax2 += rsqr(MAX(dmin, psize - dmin));
            /* sum max distance^2 */
        }
        rc = rsqrt(bmax2) / ctx->theta;      /* using max dist from cm */
    }
    else if (ctx->criterion == NEWCRITERION) /* use new criterion? */
    {
        rc = psize / ctx->theta + distv(cmpos, Pos(p));
        /* use size plus offset */
    }
    else
    {
        fail("Got unknown criterion: %d\n", ctx->criterion);
    }
    Rcrit2(p) = rsqr(rc);           /* store square of radius */
}

/* threadtree: do a recursive treewalk starting from node p,
 * with next stop n, installing Next and More links.
 */

static void threadtree(nodeptr p, nodeptr n)
{
    int ndesc, i;
    nodeptr desc[NSUB+1];

    Next(p) = n;                                /* link to next node */
    if (Type(p) == CELL)                        /* any children to thread? */
    {
        ndesc = 0;                              /* count extant children */
        for (i = 0; i < NSUB; i++)              /* loop over subnodes */
            if (Subp(p)[i] != NULL)             /* found a live one? */
                desc[ndesc++] = Subp(p)[i];     /* store in table */
        More(p) = desc[0];                      /* link to first child */
        desc[ndesc] = n;                        /* end table with next */
        for (i = 0; i < ndesc; i++)             /* loop over children */
            threadtree(desc[i], desc[i+1]);     /* thread each w/ next */
    }
}

/* hackquad: descend tree, evaluating quadrupole moments.  Note that this
 * routine is coded so that the Subp() and Quad() components of a cell can
 * share the same memory locations.
 */

static void hackquad(cellptr p)
{
    int i;
    nodeptr psub[NSUB], q;
    vector dr;
    real drsq;
    matrix drdr, Idrsq, tmpm;

    for (i = 0; i < NSUB; i++)          /* loop over subnodes */
        psub[i] = Subp(p)[i];           /* copy each to safety */
    CLRM(Quad(p));                              /* init quadrupole moment */
    for (i = 0; i < NSUB; i++)                  /* loop over subnodes */
        if ((q = psub[i]) != NULL)          /* does subnode exist? */
        {
            if (Type(q) == CELL)        /* and is it a call? */
                hackquad((cellptr) q);      /* process it first */
            SUBV(dr, Pos(q), Pos(p));           /* displacement vect. */
            OUTVP(drdr, dr, dr);                /* outer prod. of dr */
            DOTVP(drsq, dr, dr);                /* dot prod. dr * dr */
            SETMI(Idrsq);                       /* init unit matrix */
            MULMS(Idrsq, Idrsq, drsq);          /* scale by dr * dr */
            MULMS(tmpm, drdr, 3.0);             /* scale drdr by 3 */
            SUBM(tmpm, tmpm, Idrsq);            /* form quad. moment */
            MULMS(tmpm, tmpm, Mass(q));         /* from cm of subnode */
            if (Type(q) == CELL)                /* if subnode is cell */
                ADDM(tmpm, tmpm, Quad(q));      /* add its moment */
            ADDM(Quad(p), Quad(p), tmpm);       /* add to qm of cell */
        }
}

/* TODO: Ownership of bodytab */
void nbody_ctx_destroy(NBodyCtx* ctx)
{
    free(ctx->headline);
    free(ctx->outfilename);
    if (ctx->outfile)
        fclose(ctx->outfile);
}

