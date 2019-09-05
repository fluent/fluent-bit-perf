# Helper interfaces
set(src_helpers
  flb_data_file.c
  flb_report.c
  flb_proc.c
  )

# flb-tail-writer
set(src_tail_writer
  ${src_helpers}
  flb-tail-writer.c)

add_executable(flb-tail-writer ${src_tail_writer})
