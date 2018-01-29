#include <aos/aos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aos/aos_rpc.h>
#include <aos/sys_debug.h>
#include "../common/get_rpc.h"

#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>

int main(int argc, char *argv[])
{
    debug_printf("mem_if started: testing memory serving RPC\n");

    struct aos_rpc *memserv = grading_get_init_rpc();

    errval_t err;
    size_t retbytes = 0;
    struct capref cap, frame;
    err = slot_alloc(&frame);
    assert(err_is_ok(err));
    for (size_t request = 1UL << 12; request < 1UL << 21; request = request << 1) {
        debug_printf("testing with %zu bytes\n", request);
        err = aos_rpc_get_ram_cap(memserv, request, BASE_PAGE_SIZE, &cap, &retbytes);
        assert(err_is_ok(err));
        assert(retbytes >= request);
        struct frame_identity fi;
        err = invoke_frame_identify(cap, &fi);
        assert(fi.bytes == retbytes);
        err = cap_retype(frame, cap, 0, ObjType_Frame, retbytes, 1);
        assert(err_is_ok(err));
        void *vbuf;
        err = paging_map_frame(get_current_paging_state(),
                &vbuf, retbytes, frame, NULL, NULL);
        assert(err_is_ok(err));
        char *buf = vbuf;
        for (int i = 0; i < retbytes; i++) {
            buf[i] = i % 255;
        }
        sys_debug_flush_cache();
        for (int i = 0; i < retbytes; i++) {
            assert(buf[i] == i % 255);
        }
        //XXX: should unmap here!
        err = cap_delete(frame);
        assert(err_is_ok(err));
    }

    return 0;
}
