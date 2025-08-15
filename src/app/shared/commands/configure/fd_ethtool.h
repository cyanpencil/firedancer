#ifndef HEADER_fd_src_app_shared_commands_configure_fd_ethtool_h
#define HEADER_fd_src_app_shared_commands_configure_fd_ethtool_h

#include "configure.h"

FD_PROTOTYPES_BEGIN

int
fd_ethtool_device_is_bonded( char const * device );

int
fd_ethtool_ntuple_is_enabled( config_t const * config );

FD_PROTOTYPES_END

#endif /* HEADER_fd_src_app_shared_commands_configure_fd_ethtool_h */
