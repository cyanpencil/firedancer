#include "fd_ethtool.h"
#include <sys/stat.h>
#include <errno.h>

int
fd_ethtool_device_is_bonded( const char * device ) {
  char path[ PATH_MAX ];
  FD_TEST( fd_cstr_printf_check( path, PATH_MAX, NULL, "/sys/class/net/%s/bonding", device ) );
  struct stat st;
  int err = stat( path, &st );
  if( FD_UNLIKELY( err && errno != ENOENT ) )
    FD_LOG_ERR(( "error checking if device `%s` is bonded, stat(%s) failed (%i-%s)",
                 device, path, errno, fd_io_strerror( errno ) ));
  return !err;
}

int
fd_ethtool_ntuple_is_enabled( config_t const * config ) {
  /* if we're running in a network namespace, we configure ethtool on
     the virtual device as part of netns setup, not here */
  if( config->development.netns.enabled ) return 0;

  /* only enable if network stack is XDP */
  if( 0!=strcmp( config->net.provider, "xdp" ) ) return 0;

  /* only enable if rx_flow_steering mode requires ntuple */
  if( config->net.xdp.rx_flow_steering_ != FD_CONFIG_RX_FLOW_STEERING_NTUPLE ) return 0;

  return 1;
}
