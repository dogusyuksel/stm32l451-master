use libcsp::libcsp::{
    csp_accept, csp_bind, csp_buffer_free, csp_can_socketcan_open_and_add_interface, csp_close,
    csp_conf, csp_conn_dport, csp_conn_sport, csp_conn_t, csp_iface_t, csp_init, csp_listen,
    csp_packet_t, csp_prio_t_CSP_PRIO_NORM, csp_read, csp_route_work, csp_socket_s, csp_socket_t,
    CSP_ANY, CSP_ERR_NONE,
};
use std::env;
use std::ffi;
use std::process;
use std::{ptr, time::Duration};
use structopt::StructOpt;

use libcsp::csp_utils;

use tokio::time::sleep;

#[derive(Debug, StructOpt)]
#[structopt(name = "example", about = "An example of StructOpt usage.")]
struct Opt {
    /// Flag to print help message
    #[structopt(long)]
    help: bool,

    /// Optional iface name string
    #[structopt(long)]
    iface: Option<String>,

    /// Optional port u16
    #[structopt(long)]
    dest_port: Option<u16>,

    /// Optional dest_node_id u16
    #[structopt(long)]
    dest_node_id: Option<u16>,

    /// Optional source_node_id u32
    #[structopt(long)]
    source_node_id: Option<u32>,

    /// Optional iface name string
    #[structopt(long)]
    data: Option<String>,
}

fn send_packet_directly(
    hex_string: &str,
    dest_port: u16,
    dest_nodeid: u16,
) -> Result<(), Box<dyn std::error::Error>> {
    let ptr_send_bytes: *mut ffi::c_void;
    let bytes: Vec<u8>;

    if !hex_string.is_empty() {
        bytes = hex_string
            .split_whitespace()
            .filter_map(|s| u8::from_str_radix(s, 16).ok())
            .collect();
        ptr_send_bytes = bytes.as_ptr() as *mut ffi::c_void;
    } else {
        return Err(Box::new(std::io::Error::other("Both hex_string is empty")));
    }

    print!(
        "dest_port {}, dest nodeid: {} and bytes to be sent: ",
        dest_port, dest_nodeid
    );
    for byte in &bytes {
        print!("{:02X} ", byte);
    }
    println!();

    // unsafe needed because of following errors:
    // -> call to unsafe function `csp_transaction`
    // -> access to union field is unsafe
    unsafe {
        let _retval = csp_utils::csp_transaction(
            csp_prio_t_CSP_PRIO_NORM.try_into().unwrap(),
            dest_nodeid,
            dest_port.try_into().unwrap(),
            1000,
            ptr_send_bytes,
            bytes.len().try_into().unwrap(),
            ptr::null_mut(),
            0,
        );

        if _retval != i32::try_from(CSP_ERR_NONE).unwrap() {
            return Err(Box::new(std::io::Error::other("Packet cannot sent")));
        }
    }

    Ok(())
}

fn print_help(program_name: &str) {
    println!("Program Name: {}", program_name);
    println!("USAGE:");
    println!("    Systemwide Options:");
    println!("        --iface         : to pass can interface name (default is can0)");
    println!("        --dest_port     : to pass destination port (default is 29)");
    println!("        --dest_node_id  : to pass destination node id (default is 2)");
    println!("        --source_node_id: to pass source node id (default is 10)");
    println!("    Additional Options:");
    println!("        --data          : to pass hex string  (eg --data '01 02 03 04')");
    println!("            This option enables the breakglass mode directly");
    println!("            and raw bytes that passed as argument with this option will be sent to the dest_node_id and dest_port");
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let opt = Opt::from_args();
    println!("Received arguments: {:?}", opt);

    if opt.help {
        let program_name = env::args()
            .next()
            .unwrap_or_else(|| String::from("unknown"));
        print_help(&program_name);
        return Ok(());
    }

    let mut src_nodeid = 10;
    if let Some(source_node_id) = opt.source_node_id {
        src_nodeid = source_node_id;
    }

    let iface_name = opt.iface.as_deref().unwrap_or("can0");

    // unsafe needed because of following errors:
    // -> call to unsafe function `csp_init`
    // -> use of mutable static
    unsafe {
        csp_conf.version = 2;
        csp_init();
    }

    tokio::spawn(router_task());

    let mut default_iface: *mut csp_iface_t = ptr::null_mut();
    let if_name = ffi::CString::new(iface_name).unwrap();
    let if_name_def = ffi::CString::new("CAN").unwrap();

    // unsafe needed because of following errors:
    // -> call to unsafe function `csp_can_socketcan_open_and_add_interface`
    // -> dereference of raw pointer
    unsafe {
        csp_can_socketcan_open_and_add_interface(
            if_name.as_ptr(),
            if_name_def.as_ptr(),
            src_nodeid,
            1000000,
            true,
            &mut default_iface as *mut _,
        );

        (*default_iface).is_default = 1;
    }

    let mut port = 29;
    if let Some(dest_port) = opt.dest_port {
        port = dest_port;
    }

    let mut dest_nodeid = 2;
    if let Some(dest_node_id) = opt.dest_node_id {
        dest_nodeid = dest_node_id;
    }

    // Console breakglass mode is our first priority
    if opt.data.is_some() {
        let data = opt.data.as_deref().unwrap_or("");
        match send_packet_directly(data, port, dest_nodeid) {
            Ok(_) => {
                println!("Packet sent successfully");
                process::exit(0);
            }
            Err(e) => {
                eprintln!("Error sending _packet: {:?}", e);
                process::exit(1);
            }
        }
    }

    tokio::spawn(server_task());

    loop {
        sleep(Duration::from_secs(1)).await;
    }
}

async fn server_task() {
    println!("Server task started");
    unsafe {
        /* Create socket with no specific socket options, e.g. accepts CRC32, HMAC, etc. if enabled during compilation */
        let mut sock: csp_socket_t = std::mem::zeroed();

        /* Bind socket to all ports, e.g. all incoming connections will be handled here */
        csp_bind(
            (&mut sock) as *mut csp_socket_s,
            CSP_ANY.try_into().unwrap(),
        );

        /* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
        csp_listen((&mut sock) as *mut csp_socket_s, 10);

        /* Wait for connections and then process packets on the connection */
        loop {
            /* Wait for a new connection, 10000 mS timeout */
            let conn: *mut csp_conn_t = csp_accept((&mut sock) as *mut csp_socket_s, 10000);
            if conn.is_null() {
                // timeout
                continue;
            }

            /* Read packets on connection, timeout is 50 mS */
            let mut _packet: *mut csp_packet_t = std::ptr::null_mut();
            while {
                _packet = csp_read(conn, 50);
                !_packet.is_null()
            } {
                let _dport = csp_conn_dport(conn);
                let _sport = csp_conn_sport(conn);
                let _source_id = (*_packet).id.src;
                print!(
                    "Packet came from {} on dport {} - sport {} and  _packet lenth: {} and the _packet content: ",
                    _source_id, _dport, _sport, (*_packet).length as usize
                );
                for i in 0..(*_packet).length as usize {
                    print!("{:02x} ", (*_packet).__bindgen_anon_1.data[i]);
                }
                println!();

                csp_buffer_free(_packet as *mut ffi::c_void);
            }

            /* Close current connection */
            csp_close(conn);
        }
    }
}

async fn router_task() {
    unsafe {
        loop {
            csp_route_work();
        }
    }
}
