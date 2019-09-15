# Helper interfaces
set(src_helpers
  flb_data_file.c
  flb_report.c
  flb_proc.c
  flb_network.c
  )

# flb-tail-writer
set(src_tail_writer
  ${src_helpers}
  flb-tail-writer.c)

# flb-tcp-writer
set(src_tcp_writer
  ${src_helpers}
  flb-tcp-writer.c)

add_executable(flb-tail-writer ${src_tail_writer})
add_executable(flb-tcp-writer ${src_tcp_writer})
