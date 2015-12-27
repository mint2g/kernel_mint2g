#ifndef SPRDMUX_H
#define SPRDMUX_H


#define SPRDMUX_READ	0x01
#define SPRDMUX_WRITE	0x02
#define SPRDMUX_ALL	(SPRDMUX_READ | SPRDMUX_WRITE)	

struct sprdmux {
	int	id;
	int	(*io_write)(const char *buf, size_t len);
	int	(*io_read)(char *buf, size_t len);
	int	(*io_stop)(int mode);
};

int sprdmux_register(struct sprdmux *mux);
#endif
