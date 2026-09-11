idf_component_register(
    SRCS "src.c"
    INCLUDE_DIRS "."
	REQUIRES initialize external_flash
	PRIV_REQUIRES
	m274-encoder
	eink-driver
)

