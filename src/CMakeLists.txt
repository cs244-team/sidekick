find_library(LIBPCAP pcap)
find_library(LIBSODIUM sodium)

macro(add_app exec_name)
  add_executable("${exec_name}" "${exec_name}.cc")
  target_link_libraries("${exec_name}" util)
  target_link_libraries("${exec_name}" "${LIBPCAP}")
  target_link_libraries("${exec_name}" "${LIBSODIUM}")
endmacro(add_app)

add_app(sidekick_proxy)
add_app(webrtc_client)
add_app(webrtc_server)
add_app(playground)
