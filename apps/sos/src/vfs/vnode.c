/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *    The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Basic vnode support functions.
 */
#include "comm/comm.h"
#include "vfs.h"
#include "vnode.h"
#include "assert.h"

/*
 * Initialize an abstract vnode.
 */
int
vnode_init(struct vnode *vn, const struct vnode_ops *ops,
       struct fs *fs, void *fsdata)
{
    assert(vn != NULL);
    assert(ops != NULL);

    vn->vn_ops = ops;
    vn->vn_refcount = 1;
    vn->vn_fs = fs;
    vn->vn_data = fsdata;
    return 0;
}

/*
 * Destroy an abstract vnode.
 */
void
vnode_cleanup(struct vnode *vn)
{
    assert(vn->vn_refcount == 1);


    vn->vn_ops = NULL;
    vn->vn_refcount = 0;
    vn->vn_fs = NULL;
    vn->vn_data = NULL;
}


/*
 * Increment refcount.
 * Called by VOP_INCREF.
 */
void
vnode_incref(struct vnode *vn)
{
    assert(vn != NULL);

    vn->vn_refcount++;
}

/*
 * Decrement refcount.
 * Called by VOP_DECREF.
 * Calls VOP_RECLAIM if the refcount hits zero.
 */
extern struct device *excl_console_dev;
void
vnode_decref(struct vnode *vn)
{
    bool destroy;
    int result;

    assert(vn != NULL);


    assert(vn->vn_refcount > 0);
    if (vn->vn_refcount > 1) {
        vn->vn_refcount--;
        destroy = false;
    }
    else {
        /* Don't decrement; pass the reference to VOP_RECLAIM. */
        destroy = true;
    }


    // XXX very ugly way, because console dev's read is exlusive, so close should call its reclaim function to
    // release read perm.
    if (vn->vn_data == excl_console_dev)
    {
        assert(0 == VOP_RECLAIM(vn));
    }


    if (destroy) {
        printf ("vnode destroyed: %p\n", vn);
        result = VOP_RECLAIM(vn);
        if (result != 0 && result != EBUSY) {
            // XXX: lame.
            ERROR_DEBUG("vfs: Warning: VOP_RECLAIM: %s\n",
                strerror(result));
        }
    }
}

/*
 * Check for various things being valid.
 * Called before all VOP_* calls.
 */
void
vnode_check(struct vnode *v, const char *opstr)
{
    /* not safe, and not really needed to check constant fields */
    /*vfs_biglock_acquire();*/

    if (v == NULL) {
        ERROR_DEBUG("vnode_check: vop_%s: null vnode\n", opstr);
        assert(0);
    }
    if (v == (void *)0xdeadbeef) {
        ERROR_DEBUG("vnode_check: vop_%s: deadbeef vnode\n", opstr);
        assert(0);
    }

    if (v->vn_ops == NULL) {
        ERROR_DEBUG("vnode_check: vop_%s: null ops pointer\n", opstr);
        assert(0);
    }
    if (v->vn_ops == (void *)0xdeadbeef) {
        ERROR_DEBUG("vnode_check: vop_%s: deadbeef ops pointer\n", opstr);
        assert(0);
    }

    if (v->vn_ops->vop_magic != VOP_MAGIC) {
        ERROR_DEBUG("vnode_check: vop_%s: ops with bad magic number %lx\n",
              opstr, v->vn_ops->vop_magic);
        assert(0);
    }

    // Device vnodes have null fs pointers.
    //if (v->vn_fs == NULL) {
    //    panic("vnode_check: vop_%s: null fs pointer\n", opstr);
    //}
    if (v->vn_fs == (void *)0xdeadbeef) {
        ERROR_DEBUG("vnode_check: vop_%s: deadbeef fs pointer\n", opstr);
        assert(0);
    }


    if (v->vn_refcount < 0) {
        ERROR_DEBUG("vnode_check: vop_%s: negative refcount %d\n", opstr,
              v->vn_refcount);
        assert(0);
    }
    else if (v->vn_refcount == 0) {
        ERROR_DEBUG("vnode_check: vop_%s: zero refcount\n", opstr);
        assert(0);
    }
    else if (v->vn_refcount > 0x100000) {
        ERROR_DEBUG("vnode_check: vop_%s: warning: large refcount %d\n",
            opstr, v->vn_refcount);
        assert(0);
    }

    /*vfs_biglock_release();*/
}
