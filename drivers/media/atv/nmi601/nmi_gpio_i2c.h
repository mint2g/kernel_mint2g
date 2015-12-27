

//enternal interface


int nmi_i2c_init(void);
void nmi_i2c_deinit(void);
int nmi_i2c_read(unsigned char, unsigned char *, unsigned long);
int nmi_i2c_write(unsigned char, unsigned char *, unsigned long); 

