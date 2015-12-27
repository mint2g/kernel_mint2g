/*                                
 * types.h                        
 *                                
 *  Created on: Aug 25, 2010      
 *      Author: klabadi & dlopo   
 */                               
                                  
#ifndef TYPES_H_                  
#define TYPES_H_
#include <linux/types.h> 
/*
typedef unsigned char   u8;       
typedef unsigned short  u16;      
typedef unsigned int    u32;      
typedef void * mutex_t;           
*/
#define BIT(n)      (1 << n)      
                                  
typedef void (*handler_t)(void *);
typedef void* (*thread_t)(void *);
                                  
#endif /* TYPES_H_ */             
                                        