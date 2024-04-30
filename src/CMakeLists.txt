macro(add_app exec_name)
  add_executable("${exec_name}" "${exec_name}.cc")
  target_link_libraries("${exec_name}" util)
endmacro(add_app)

add_app(sidekick_proxy)
add_app(webrtc_client)
add_app(webrtc_server)
add_app(playground)

find_library(LIBPCAP pcap)
target_link_libraries(sidekick_proxy "${LIBPCAP}")