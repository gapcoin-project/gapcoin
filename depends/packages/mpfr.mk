package=mpfr
$(package)_version=4.1.0
$(package)_download_path=https://www.mpfr.org/mpfr-current
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=feced2d430dd5a97805fa289fed3fc8ff2b094c02d05287fd6133e7f1f0ec926
$(package)_dependencies=gmp

define $(package)_set_vars
  $(package)_config_opts=--disable-shared CC_FOR_BUILD=$(build_CC)
  $(package)_config_opts_x86_64_darwin=--with-pic
  $(package)_config_opts_x86_64_linux=--with-pic
  $(package)_config_opts_arm_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
