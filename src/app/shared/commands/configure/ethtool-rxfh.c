#include "configure.h"
#include "fd_ethtool.h"
#include <errno.h>
#include <sys/ioctl.h> /* ioctl(2) */
#include <linux/if.h> /* struct ifreq */
#include <netinet/ip.h> /* IPPROTO_IP */
#include <linux/ethtool.h> /* ETHTOOL_* */
#include <linux/sockios.h> /* SIOCETHTOOL */
#include <unistd.h> /* close(2) */

#define NAME "ethtool-rxfh"

static int
enabled( config_t const * config ) {
  return fd_ethtool_ntuple_is_enabled( config );
}

static void
init_perm( fd_cap_chk_t *   chk,
           config_t const * config FD_PARAM_UNUSED ) {
  fd_cap_chk_root( chk, NAME, "set up flow steering with `ethtool --set-rxfh-indir`" );
}

static configure_result_t
check_device( char const * device ) {
  int sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );
  if( FD_UNLIKELY( sock<0 ) ) FD_LOG_ERR(( "socket(AF_INET,SOCK_DGRAM,IPPROTO_IP) failed (%i-%s)", errno, fd_io_strerror( errno ) ));

  struct ifreq ifr = {0};
  fd_cstr_fini( fd_cstr_append_cstr( fd_cstr_init( ifr.ifr_name ), device ) );

  /* Get size of RXFH indirection table */
  struct ethtool_rxfh_indir rxfh_peek = {
    .cmd  = ETHTOOL_GRXFHINDIR,
    .size = 0
  };
  ifr.ifr_data = (void *)&rxfh_peek;
  if( FD_UNLIKELY( ioctl( sock, SIOCETHTOOL, &ifr ) ) ) {
    if( errno==ENOTSUP ) {
      FD_LOG_ERR(( "error configuring network device `%s`: ethtool rxfh not supported by device.\n"
                   "Consider changing [net.xdp.rx_flow_steering] to \"fewer-queues\".",
                   device ));
    }
    FD_LOG_ERR(( "error configuring network device `%s`: ioctl(SIOCETHTOOL,ETHTOOL_GRXFHINDIR) failed (%i-%s)",
                 device, errno, fd_io_strerror( errno ) ));
  }

  /* Actually download RXFH indirection table */
  ulong const rxfh_max = rxfh_peek.size;
  if( FD_UNLIKELY( !rxfh_max ) ) {
    FD_LOG_ERR(( "error configuring network device `%s`: ethtool RXFH indirection table has no entries",
                 device ));
  }
  struct ethtool_rxfh_indir * rxfh_get = calloc( 1UL, sizeof(struct ethtool_rxfh_indir) + rxfh_max*sizeof(uint) );
  if( FD_UNLIKELY( !rxfh_get ) ) FD_LOG_ERR(( "out of memory" ));
  rxfh_get->cmd  = ETHTOOL_GRXFHINDIR;
  rxfh_get->size = (uint)rxfh_max;
  ifr.ifr_data   = (void *)rxfh_get;
  if( FD_UNLIKELY( ioctl( sock, SIOCETHTOOL, &ifr ) ) ) {
    free( rxfh_get );
    FD_LOG_ERR(( "error configuring network device `%s`: ioctl(SIOCETHTOOL,ETHTOOL_GRXFHINDIR) failed (%i-%s)",
                 device, errno, fd_io_strerror( errno ) ));
  }

  if( FD_UNLIKELY( close( sock ) ) ) {
    FD_LOG_ERR(( "close(socket) failed (%i-%s)", errno, fd_io_strerror( errno ) ));
  }

  /* Ensure that default RXFH indir table doesn't steer to queue zero */
  int ok = 1;
  ulong const rxfh_cnt = fd_ulong_min( rxfh_get->size, rxfh_max );
  for( ulong i=0UL; i<rxfh_cnt; i++ ) {
    if( rxfh_get->ring_index[ i ]==0U ) ok = 0;
  }
  free( rxfh_get );

  /* FIXME verify that RXFH indir table load balances uniformly across
     all channels */

  if( !ok ) NOT_CONFIGURED( "device `%s` rxfh-indir table is not set up (queue 0 is not isolated from default flow steering rules)", device );
  else      CONFIGURE_OK();
}

static configure_result_t
check( config_t const * config ) {
  char const * const device = config->net.interface;

  if( FD_UNLIKELY( fd_ethtool_device_is_bonded( device ) ) ) {
    FD_LOG_ERR(( "device `%s` does not support [net.xdp.rx_flow_steering] mode \"ntuple\": device is bonded",
                 device ));
  }

  return check_device( device );
}

static void
init( config_t const * config ) {
  (void)config;
  return;
}

configure_stage_t fd_cfg_stage_ethtool_rxfh = {
  .name            = NAME,
  .always_recreate = 0,
  .enabled         = enabled,
  .init_perm       = init_perm,
  .fini_perm       = NULL,
  .init            = init,
  .fini            = NULL,
  .check           = check
};

#undef NAME
