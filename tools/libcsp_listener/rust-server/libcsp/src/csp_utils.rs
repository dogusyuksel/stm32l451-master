use std::ffi;

use crate::libcsp::{
    csp_buffer_free, csp_buffer_get, csp_close, csp_conn_t, csp_connect, csp_read, csp_send,
    csp_transaction_persistent, CSP_ERR_NONE, CSP_ERR_TX, CSP_O_NONE,
};

/// Performs a CSP transaction.
///
/// # Arguments
///
/// * `prio` - Priority of the transaction
/// * `dest` - Destination address
/// * `port` - Port number
/// * `timeout` - Timeout in milliseconds
/// * `out_buf` - Pointer to the output buffer
/// * `out_len` - Length of the output buffer
/// * `in_buf` - Pointer to the input buffer
/// * `in_len` - Length of the input buffer
///
/// # Safety
///
/// This function is unsafe because it dereferences raw pointers (`out_buf` and `in_buf`).
/// The caller must ensure that:
/// - The pointers are valid and properly aligned.
/// - The memory regions pointed to by `out_buf` and `in_buf` are valid for the lengths
///   specified by `out_len` and `in_len` respectively.
/// - The memory regions do not overlap.
/// - The lifetimes of the pointed-to memory are at least as long as the duration of this function call.
///
/// # Returns
///
/// Returns the result of the transaction as an i32.
#[allow(clippy::too_many_arguments)]
pub unsafe fn csp_transaction(
    prio: u8,
    dest: u16,
    port: u8,
    timeout: u32,
    out_buf: *mut ffi::c_void,
    out_len: u16,
    in_buf: *mut ffi::c_void,
    in_len: u16,
) -> i32 {
    unsafe {
        let conn: *mut csp_conn_t = csp_connect(prio, dest, port, 0, CSP_O_NONE);
        if conn.is_null() {
            return CSP_ERR_TX;
        }

        if !in_buf.is_null() {
            let ret_val: i32 = csp_transaction_persistent(
                conn,
                timeout,
                out_buf,
                out_len.into(),
                in_buf,
                in_len.into(),
            );
            csp_close(conn);
            return ret_val;
        }

        let mut packet = csp_buffer_get(0);
        if packet.is_null() {
            return CSP_ERR_TX;
        }

        (*packet).length = out_len;
        (*packet).__bindgen_anon_1.data = *(out_buf as *const [u8; 256]);
        csp_send(conn, packet);

        packet = csp_read(conn, timeout);
        if packet.is_null() {
            csp_buffer_free(packet as *mut ffi::c_void);
            csp_close(conn);
            return CSP_ERR_TX;
        }

        print!(
            "Incoming packet lenth: {} and the packet content: ",
            (*packet).length as usize
        );
        for i in 0..(*packet).length as usize {
            print!("{:02x} ", (*packet).__bindgen_anon_1.data[i]);
        }
        println!();

        csp_buffer_free(packet as *mut ffi::c_void);
        csp_close(conn);
        CSP_ERR_NONE.try_into().unwrap()
    }
}
