                                                                                                                                                                       
#include "mipi_dsih_dphy.h"                                                                                                                                            
#define PRECISION_FACTOR        (1000)                                                                                                                                 
/* Reference clock frequency divided by Input Frequency Division Ratio LIMITS */                                                                                       
#define DPHY_DIV_UPPER_LIMIT    (40000)                                                                                                                                
#define DPHY_DIV_LOWER_LIMIT    (1000)                                                                                                                                 
                                                                                                                                                                       
#ifdef DWC_MIPI_DPHY_BIDIR_TSMC40LP                                                                                                                                    
#define MIN_OUTPUT_FREQ         (80)                                                                                                                                   
#elif defined DPHY2Btql                                                                                                                                                
#define MIN_OUTPUT_FREQ         (200)                                                                                                                                  
#endif                                                                                                                                                                 
                                                                                                                                                                       
dsih_error_t mipi_dsih_dphy_open(dphy_t * phy)                                                                                                                         
{                                                                                                                                                                      
    if (phy == 0)                                                                                                                                                      
    {                                                                                                                                                                  
        return ERR_DSI_PHY_INVALID;                                                                                                                                    
    }                                                                                                                                                                  
    else if ((phy->core_read_function == 0) || (phy->core_write_function == 0))                                                                                        
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_IO;                                                                                                                                     
    }                                                                                                                                                                  
    else if (phy->status == INITIALIZED)                                                                                                                               
    {                                                                                                                                                                  
        return ERR_DSI_PHY_INVALID;                                                                                                                                    
    }                                                                                                                                                                  
    phy->status = NOT_INITIALIZED;                                                                                                                                     
    mipi_dsih_dphy_reset(phy, 0);                                                                                                                                      
    mipi_dsih_dphy_stop_wait_time(phy, 0x1C);                                                                                                                          
    mipi_dsih_dphy_no_of_lanes(phy, 1);                                                                                                                                
    mipi_dsih_dphy_clock_en(phy, 1);                                                                                                                                   
    mipi_dsih_dphy_shutdown(phy, 1);                                                                                                                                   
    mipi_dsih_dphy_reset(phy, 1);                                                                                                                                      
    phy->status = INITIALIZED;                                                                                                                                         
    return OK;                                                                                                                                                         
}                                                                                                                                                                      
dsih_error_t mipi_dsih_dphy_configure(dphy_t * phy, uint8_t no_of_lanes, uint32_t output_freq)                                                                         
{                                                                                                                                                                      
    uint32_t loop_divider = 0; /* (M) */                                                                                                                               
    uint32_t input_divider = 1; /* (N) */                                                                                                                              
    uint8_t data[4]; /* maximum data for now are 4 bytes per test mode*/                                                                                               
    uint8_t no_of_bytes = 0;                                                                                                                                           
    uint8_t i = 0; /* iterator */                                                                                                                                      
    uint8_t range = 0; /* ranges iterator */                                                                                                                           
    int flag = 0;                                                                                                                                                      
#ifdef DWC_MIPI_DPHY_BIDIR_TSMC40LP                                                                                                                                    
    struct                                                                                                                                                             
    {                                                                                                                                                                  
        uint32_t freq;  /* upper margin of frequency range */                                                                                                          
        uint8_t hs_freq; /* hsfreqrange */                                                                                                                             
        uint8_t vco_range; /* vcorange */                                                                                                                              
    }                                                                                                                                                                  
    ranges[] =                                                                                                                                                         
    {                                                                                                                                                                  
        {90, 0x00, 0x01}, {100, 0x10, 0x01}, {110, 0x20, 0x01},                                                                                                        
        {125, 0x01, 0x01}, {140, 0x11, 0x01}, {150, 0x21, 0x01},                                                                                                       
        {160, 0x02, 0x01}, {180, 0x12, 0x03}, {200, 0x22, 0x03},                                                                                                       
        {210, 0x03, 0x03}, {240, 0x13, 0x03}, {250, 0x23, 0x03},                                                                                                       
        {270, 0x04, 0x07}, {300, 0x14, 0x07}, {330, 0x24, 0x07},                                                                                                       
        {360, 0x15, 0x07}, {400, 0x25, 0x07}, {450, 0x06, 0x07},                                                                                                       
        {500, 0x16, 0x07}, {550, 0x07, 0x0f}, {600, 0x17, 0x0f},                                                                                                       
        {650, 0x08, 0x0f}, {700, 0x18, 0x0f}, {750, 0x09, 0x0f},                                                                                                       
        {800, 0x19, 0x0f}, {850, 0x0A, 0x0f}, {900, 0x1A, 0x0f},                                                                                                       
        {950, 0x2A, 0x0f}, {1000, 0x3A, 0x0f}                                                                                                                          
    };                                                                                                                                                                 
    struct                                                                                                                                                             
    {                                                                                                                                                                  
        uint32_t loop_div; /* upper limit of loop divider range */                                                                                                     
        uint8_t cp_current; /* icpctrl */                                                                                                                              
        uint8_t lpf_resistor; /* lpfctrl */                                                                                                                            
    }                                                                                                                                                                  
    loop_bandwidth[] =                                                                                                                                                 
    {                                                                                                                                                                  
        {32, 0x06, 0x10}, {64, 0x06, 0x10}, {128, 0x0C, 0x08},                                                                                                         
        {256, 0x04, 0x04}, {512, 0x00, 0x01}, {768, 0x01, 0x01},                                                                                                       
        {1000, 0x02, 0x01}                                                                                                                                             
    };                                                                                                                                                                 
#elif defined DPHY2Btql                                                                                                                                                
    struct                                                                                                                                                             
    {                                                                                                                                                                  
        uint32_t loop_div; /* upper limit of loop divider range */                                                                                                     
        uint8_t cp_current; /* icpctrl */                                                                                                                              
        uint8_t lpf_resistor; /* lpfctrl */                                                                                                                            
    }                                                                                                                                                                  
    loop_bandwidth[] =                                                                                                                                                 
    {                                                                                                                                                                  
        {32, 0x0B, 0x00}, {64, 0x0A, 0x00}, {128, 0x09, 0x01},                                                                                                         
        {256, 0x08, 0x03}, {512, 0x08, 0x07}, {768, 0x08, 0x0F},                                                                                                       
        {1000, 0x08, 0x1F}                                                                                                                                             
    };                                                                                                                                                                 
#endif                                                                                                                                                                 
    if (phy == 0)                                                                                                                                                      
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_INSTANCE;                                                                                                                               
    }                                                                                                                                                                  
    if (phy->status < INITIALIZED)                                                                                                                                     
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_INSTANCE;                                                                                                                               
    }                                                                                                                                                                  
    if (output_freq < MIN_OUTPUT_FREQ)                                                                                                                                 
    {                                                                                                                                                                  
        return ERR_DSI_PHY_FREQ_OUT_OF_BOUND;                                                                                                                          
    }                                                                                                                                                                  
    /* find M and N dividers */                                                                                                                                        
    for (input_divider = 1 + (phy->reference_freq / DPHY_DIV_UPPER_LIMIT); ((phy->reference_freq / input_divider) >= DPHY_DIV_LOWER_LIMIT) && (!flag); input_divider++)
    {   /* here the >= DPHY_DIV_LOWER_LIMIT is a phy constraint, formula should be above 1 MHz */                                                                      
        if (((output_freq * input_divider) % (phy->reference_freq )) == 0)                                                                                             
        {   /* values found */                                                                                                                                         
            loop_divider = ((output_freq * input_divider) / (phy->reference_freq ));                                                                                   
            if (loop_divider >= 12)                                                                                                                                    
            {                                                                                                                                                          
                flag = 1;                                                                                                                                              
            }                                                                                                                                                          
        }                                                                                                                                                              
    }                                                                                                                                                                  
    if ((!flag) || ((phy->reference_freq / input_divider) < DPHY_DIV_LOWER_LIMIT))                                                                                     
    {   /* no exact value found in previous for loop */                                                                                                                
        /* this solution is not favourable as jitter would be maximum */                                                                                               
        loop_divider = output_freq / DPHY_DIV_LOWER_LIMIT;                                                                                                             
        input_divider = phy->reference_freq / DPHY_DIV_LOWER_LIMIT;                                                                                                    
    }                                                                                                                                                                  
    else                                                                                                                                                               
    {   /* variable was incremented before exiting the loop */                                                                                                         
        input_divider--;                                                                                                                                               
    }                                                                                                                                                                  
    for (i = 0; (i < (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) && (loop_divider > loop_bandwidth[i].loop_div); i++)                                          
    {                                                                                                                                                                  
        ;                                                                                                                                                              
    }                                                                                                                                                                  
    if (i >= (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0])))                                                                                                       
    {                                                                                                                                                                  
        return ERR_DSI_PHY_FREQ_OUT_OF_BOUND;                                                                                                                          
    }                                                                                                                                                                  
    /* get the PHY in power down mode (shutdownz=0) and reset it (rstz=0) to                                                                                           
    avoid transient periods in PHY operation during re-configuration procedures. */                                                                                    
    mipi_dsih_dphy_reset(phy, 0);                                                                                                                                      
    mipi_dsih_dphy_clock_en(phy, 0);                                                                                                                                   
    mipi_dsih_dphy_shutdown(phy, 0);                                                                                                                                   
    /* provide an initial active-high test clear pulse in TESTCLR  */                                                                                                  
    mipi_dsih_dphy_test_clear(phy, 1);                                                                                                                                 
    mipi_dsih_dphy_test_clear(phy, 0);                                                                                                                                 
#ifdef DWC_MIPI_DPHY_BIDIR_TSMC40LP                                                                                                                                    
    /* find ranges */                                                                                                                                                  
    for (range = 0; (range < (sizeof(ranges)/sizeof(ranges[0]))) && ((output_freq / 1000) > ranges[range].freq); range++)                                              
    {                                                                                                                                                                  
        ;                                                                                                                                                              
    }                                                                                                                                                                  
    if (range >= (sizeof(ranges)/sizeof(ranges[0])))                                                                                                                   
    {                                                                                                                                                                  
        return ERR_DSI_PHY_FREQ_OUT_OF_BOUND;                                                                                                                          
    }                                                                                                                                                                  
    /* set up board depending on environment if any */                                                                                                                 
    if (phy->bsp_pre_config != 0)                                                                                                                                      
    {                                                                                                                                                                  
        phy->bsp_pre_config(phy, 0);                                                                                                                                   
    }

  /* Jessica add - begin*/  
    data[0] =  0x41;//0x44;//0x44;//0x40;                 //0x40: ok for 200    clock lane lpx /*about 52ns*/
    mipi_dsih_dphy_write(phy, 0x60, data, 1);
    data[0] =  0x0; //0xA6;//0xC6;//0xC6;//0x86;                 //0x48: ok for 200     prepare time
    mipi_dsih_dphy_write(phy, 0x61, data, 1);

    data[0] =  0x0;//0x6a;//0x6a;//0x4a;                  //0x4a: ok for 200    zero time
    mipi_dsih_dphy_write(phy, 0x62, data, 1);

    data[0] =  0x41;//0x44;//0x40;//0x40;              // 0x40: ok for 200          data lane lpx /*about 52ns*/
    mipi_dsih_dphy_write(phy, 0x70, data, 1);

    data[0] = 0x0;// 0x84;//0x96;//0x96;//0x86;                //0x48: ok for 200         prepare time
    mipi_dsih_dphy_write(phy, 0x71, data, 1);

    data[0] =  0x0;//0x44;//0x44;//0x40;               //0x4a: ok for 200          zero time
    mipi_dsih_dphy_write(phy, 0x72, data, 1);

    //data[0] =  0x44;                                                                                 
    //mipi_dsih_dphy_write(phy, 0x73, data, 1);	

    //data[0] =  0x7F;                                                                                 
    //mipi_dsih_dphy_write(phy, 0x74, data, 1);

  /* Jessica add - end*/  

    /* setup digital part */                                                                                                                                           
    /* hs frequency range [7]|[6:1]|[0]*/                                                                                                                              
    data[0] = (0 << 7) | (ranges[range].hs_freq << 1) | 0;    
   //data[0] = (0 << 7) | (0x23 << 1) | 0; 
   /*From ASIC, we need unmask this code to make the frequency correct*/
    mipi_dsih_dphy_write(phy, 0x44, data, 1);       //Jessica remove for more accurate frequency
    /* setup PLL */                                                                                                                                                    
    /* vco range  [7]|[6:3]|[2:1]|[0] */                                                                                                                               
    data[0] = (1 << 7) | (ranges[range].vco_range << 3) | (0 << 1) | 0;                                                                                                
    mipi_dsih_dphy_write(phy, 0x10, data, 1);                 //Jessica                                                                                                         
    /* PLL  reserved|Input divider control|Loop Divider Control|Post Divider Ratio [7:6]|[5]|[4]|[3:0] */                                                              
    data[0] = (0x00 << 6) | (0x01 << 5) | (0x01 << 4) | (0x03 << 0); /* post divider default = 0x03 - it is only used for clock out 2*/                                
    mipi_dsih_dphy_write(phy, 0x19, data, 1);      //Jessica                                                                                                                    
#elif defined DPHY2Btql                                                                                                                                                
    /* vco range  [7:5]|[4]|[3]|[2:1]|[0] */                                                                                                                           
    data[0] =  ((((output_freq / 1000) > 500 )? 1: 0) << 4) | (1 << 3) | (0 << 1) | 0;                                                                                 
    mipi_dsih_dphy_write(phy, 0x10, data, 1);                                                                                                                          
#endif                                                                                                                                                                 
    /* PLL Lock bypass|charge pump current [7:4]|[3:0] */                                                                                                              
    data[0] = (0x00 << 4) | (loop_bandwidth[i].cp_current << 0);                                                                                                       
    mipi_dsih_dphy_write(phy, 0x11, data, 1);           //Jessica                                                                                                               
    /* bypass CP default|bypass LPF default| LPF resistor [7]|[6]|[5:0] */                                                                                             
    data[0] = (0x01 << 7) | (0x01 << 6) |(loop_bandwidth[i].lpf_resistor << 0);                                                                                        
    mipi_dsih_dphy_write(phy, 0x12, data, 1);                                                                                                                          
    /* PLL input divider ratio [7:0] */                                                                                                                                
   data[0] = input_divider - 1;                                                                                                                                       
   mipi_dsih_dphy_write(phy, 0x17, data, 1);           //Jessica                                                                                                               
    
    data[0] = 1;
    mipi_dsih_dphy_write(phy, 0xB0, data, 1);



    no_of_bytes = 2; /* pll loop divider (code 0x18) takes only 2 bytes (10 bits in data) */                                                                           
    for (i = 0; i < no_of_bytes; i++)                                                                                                                                  
    {                                                                                                                                                                  
        data[i] = ((uint8_t)((((loop_divider - 1) >> (5 * i)) & 0x1F) | (i << 7) ));                                                                                   
        /* 7 is dependent on no_of_bytes                                                                                                                               
        make sure 5 bits only of value are written at a time */                                                                                                        
    }                                                                                                                                                                  
    /* PLL loop divider ratio - SET no|reserved|feedback divider [7]|[6:5]|[4:0] */                                                                                    
    mipi_dsih_dphy_write(phy, 0x18, data, no_of_bytes);                                                                                                                
    mipi_dsih_dphy_no_of_lanes(phy, no_of_lanes);                                                                                                                      
    mipi_dsih_dphy_stop_wait_time(phy, 0x1C);                                                                                                                          
    mipi_dsih_dphy_clock_en(phy, 1);                                                                                                                                   
    mipi_dsih_dphy_shutdown(phy, 1);                                                                                                                                   
    mipi_dsih_dphy_reset(phy, 1);                                                                                                                                      
    return OK;                                                                                                                                                         
} 


dsih_error_t mipi_dsih_dphy_close(dphy_t * phy)                                                                                                                        
{                                                                                                                                                                      
    if (phy == 0)                                                                                                                                                      
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_INSTANCE;                                                                                                                               
    }                                                                                                                                                                  
    else if ((phy->core_read_function == 0) || (phy->core_write_function == 0))                                                                                        
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_IO;                                                                                                                                     
    }                                                                                                                                                                  
    if (phy->status < NOT_INITIALIZED)                                                                                                                                 
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_INSTANCE;                                                                                                                               
    }                                                                                                                                                                  
    mipi_dsih_dphy_reset(phy, 0);                                                                                                                                      
    mipi_dsih_dphy_reset(phy, 1);                                                                                                                                      
    mipi_dsih_dphy_shutdown(phy, 0);                                                                                                                                   
    phy->status = NOT_INITIALIZED;                                                                                                                                     
    return OK;                                                                                                                                                         
}                                                                                                                                                                      
void mipi_dsih_dphy_clock_en(dphy_t * instance, int en)                                                                                                                
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_RSTZ, en, 2, 1);                                                                                                
}                                                                                                                                                                      
void mipi_dsih_dphy_reset(dphy_t * instance, int reset)                                                                                                                
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_RSTZ, reset, 1, 1);                                                                                             
}                                                                                                                                                                      
void mipi_dsih_dphy_shutdown(dphy_t * instance, int powerup)                                                                                                           
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_RSTZ, powerup, 0, 1);                                                                                           
}                                                                                                                                                                      
void mipi_dsih_dphy_stop_wait_time(dphy_t * instance, uint8_t no_of_byte_cycles)                                                                                       
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CFG, no_of_byte_cycles, 2, 8);                                                                               
}                                                                                                                                                                      
void mipi_dsih_dphy_no_of_lanes(dphy_t * instance, uint8_t no_of_lanes)                                                                                                
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CFG, no_of_lanes - 1, 0, 2);                                                                                 
}                                                                                                                                                                      
uint8_t mipi_dsih_dphy_get_no_of_lanes(dphy_t * instance)                                                                                                              
{                                                                                                                                                                      
    return mipi_dsih_dphy_read_part(instance, R_DSI_HOST_PHY_IF_CFG, 0, 2);                                                                                            
}                                                                                                                                                                      
void mipi_dsih_dphy_enable_hs_clk(dphy_t * instance, int enable)                                                                                                       
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, enable, 0, 1);                                                                                         
}                                                                                                                                                                      
dsih_error_t mipi_dsih_dphy_escape_mode_trigger(dphy_t * instance, uint8_t trigger_request)                                                                            
{                                                                                                                                                                      
    uint8_t sum = 0;                                                                                                                                                   
    int i = 0;                                                                                                                                                         
    for (i = 0; i < 4; i++)                                                                                                                                            
    {                                                                                                                                                                  
        sum += ((trigger_request >> i) & 1);                                                                                                                           
    }                                                                                                                                                                  
    if (sum == 1)                                                                                                                                                      
    {   /* clear old trigger */                                                                                                                                        
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0x00, 5, 4);                                                                                       
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, trigger_request, 5, 4);                                                                            
        for (i = 0; i < DSIH_PHY_ACTIVE_WAIT; i++)                                                                                                                     
        {                                                                                                                                                              
            if(mipi_dsih_dphy_status(instance, 0x0010))                                                                                                                
            {                                                                                                                                                          
                break;                                                                                                                                                 
            }                                                                                                                                                          
        }                                                                                                                                                              
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0x00, 5, 4);                                                                                       
        if (i >= DSIH_PHY_ACTIVE_WAIT)                                                                                                                                 
        {                                                                                                                                                              
            return ERR_DSI_TIMEOUT;                                                                                                                                    
        }                                                                                                                                                              
        return OK;                                                                                                                                                     
    }                                                                                                                                                                  
    return ERR_DSI_INVALID_COMMAND;                                                                                                                                    
}                                                                                                                                                                      
void mipi_dsih_dphy_ulps_data_lanes(dphy_t * instance, int enable)                                                                                                     
{                                                                                                                                                                      
    int timeout;                                                                                                                                                       
    if (enable)                                                                                                                                                        
    {                                                                                                                                                                  
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 1, 3, 1);                                                                                          
    }                                                                                                                                                                  
    else                                                                                                                                                               
    {                                                                                                                                                                  
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 1, 4, 1);                                                                                          
        for (timeout = 0; timeout < DSIH_PHY_ACTIVE_WAIT; timeout++)                                                                                                   
        {   /* verify that the DPHY has left ULPM */                                                                                                                   
            /* mask 1010100100000 */                                                                                                                                   
            if (mipi_dsih_dphy_status(instance, 0x1520) == 0)                                                                                                          
            {   /* wait at least 1ms */                                                                                                                                
                for (timeout = 0; timeout < ONE_MS_ACTIVE_WAIT; timeout++)                                                                                             
                {                                                                                                                                                      
                    ;                                                                                                                                                  
                }                                                                                                                                                      
                break;                                                                                                                                                 
            }                                                                                                                                                          
        }                                                                                                                                                              
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0, 3, 1);                                                                                          
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0, 4, 1);                                                                                          
    }                                                                                                                                                                  
}                                                                                                                                                                      
void mipi_dsih_dphy_ulps_clk_lane(dphy_t * instance, int enable)                                                                                                       
{                                                                                                                                                                      
    int timeout;                                                                                                                                                       
    if (enable)                                                                                                                                                        
    {                                                                                                                                                                  
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0, 0, 1);                                                                                          
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 1, 1, 1);                                                                                          
    }                                                                                                                                                                  
    else                                                                                                                                                               
    {                                                                                                                                                                  
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 1, 2, 1);                                                                                          
        for (timeout = 0; timeout < DSIH_PHY_ACTIVE_WAIT; timeout++)                                                                                                   
        {   /* verify that the DPHY has left ULPM */                                                                                                                   
            /* mask 1010100100000 */                                                                                                                                   
            if (mipi_dsih_dphy_status(instance, 0x0004) == 0)                                                                                                          
            {   /* wait at least 1ms */                                                                                                                                
                for (timeout = 0; timeout < ONE_MS_ACTIVE_WAIT; timeout++)                                                                                             
                {                                                                                                                                                      
                    ;                                                                                                                                                  
                }                                                                                                                                                      
                break;                                                                                                                                                 
            }                                                                                                                                                          
        }                                                                                                                                                              
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0, 1, 1);                                                                                          
        mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_IF_CTRL, 0, 2, 1);                                                                                          
    }                                                                                                                                                                  
}                                                                                                                                                                      
uint32_t mipi_dsih_dphy_status(dphy_t * instance, uint16_t mask)                                                                                                       
{                                                                                                                                                                      
    return mipi_dsih_dphy_read_word(instance, R_DSI_HOST_PHY_STATUS) & mask;                                                                                           
}                                                                                                                                                                      
void mipi_dsih_dphy_test_clock(dphy_t * instance, int value)                                                                                                           
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_TST_CRTL0, value, 1, 1);                                                                                        
}                                                                                                                                                                      
void mipi_dsih_dphy_test_clear(dphy_t * instance, int value)                                                                                                           
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_TST_CRTL0, value, 0, 1);                                                                                        
}                                                                                                                                                                      
void mipi_dsih_dphy_test_en(dphy_t * instance, uint8_t on_falling_edge)                                                                                                
{                                                                                                                                                                      
    mipi_dsih_dphy_write_part(instance, R_DSI_HOST_PHY_TST_CRTL1, on_falling_edge, 16, 1);                                                                             
}                                                                                                                                                                      
uint8_t mipi_dsih_dphy_test_data_out(dphy_t * instance)                                                                                                                
{                                                                                                                                                                      
    return mipi_dsih_dphy_read_part(instance, R_DSI_HOST_PHY_TST_CRTL1, 8, 8);                                                                                         
}                                                                                                                                                                      
void mipi_dsih_dphy_test_data_in(dphy_t * instance, uint8_t test_data)                                                                                                 
{                                                                                                                                                                      
    mipi_dsih_dphy_write_word(instance, R_DSI_HOST_PHY_TST_CRTL1, test_data);                                                                                          
}                                                                                                                                                                      
void mipi_dsih_dphy_write(dphy_t * instance, uint8_t address, uint8_t * data, uint8_t data_length)                                                                     
{                                                                                                                                                                      
    unsigned i = 0;                                                                                                                                                    
    if (data != 0)                                                                                                                                                     
    {                                                                                                                                                                  
#if ((defined DWC_MIPI_DPHY_BIDIR_TSMC40LP) || (defined DPHY2Btql))                                                                                                    
        /* set the TESTCLK input high in preparation to latch in the desired test mode */                                                                              
        mipi_dsih_dphy_test_clock(instance, 1);                                                                                                                        
        /* set the desired test code in the input 8-bit bus TESTDIN[7:0] */                                                                                            
        mipi_dsih_dphy_test_data_in(instance, address);                                                                                                                
        /* set TESTEN input high  */                                                                                                                                   
        mipi_dsih_dphy_test_en(instance, 1);                                                                                                                           
        /* drive the TESTCLK input low; the falling edge captures the chosen test code into the transceiver */                                                         
        mipi_dsih_dphy_test_clock(instance, 0);                                                                                                                        
        /* set TESTEN input low to disable further test mode code latching  */                                                                                         
        mipi_dsih_dphy_test_en(instance, 0);                                                                                                                           
        /* start writing MSB first */                                                                                                                                  
        for (i = data_length; i > 0; i--)                                                                                                                              
        {   /* set TESTDIN[7:0] to the desired test data appropriate to the chosen test mode */                                                                        
            mipi_dsih_dphy_test_data_in(instance, data[i - 1]);                                                                                                        
            /* pulse TESTCLK high to capture this test data into the macrocell; repeat these two steps as necessary */                                                 
            mipi_dsih_dphy_test_clock(instance, 1);                                                                                                                    
            mipi_dsih_dphy_test_clock(instance, 0);                                                                                                                    
        }                                                                                                                                                              
#endif                                                                                                                                                                 
    }                                                                                                                                                                  
}                                                                                                                                                                      
                                                                                                                                                                       
                                                                                                                                                                       
                                                                                                                                                                       
/* abstracting BSP */                                                                                                                                                  
void mipi_dsih_dphy_write_word(dphy_t * instance, uint32_t reg_address, uint32_t data)                                                                                 
{                                                                                                                                                                      
    if (instance->core_write_function != 0)                                                                                                                            
    {                                                                                                                                                                  
        instance->core_write_function(instance->address, reg_address, data);                                                                                           
    }                                                                                                                                                                  
}                                                                                                                                                                      
void mipi_dsih_dphy_write_part(dphy_t * instance, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width)                                                   
{                                                                                                                                                                      
    uint32_t mask = 0;                                                                                                                                                 
    uint32_t temp = 0;                                                                                                                                                 
    if (instance->core_read_function != 0)                                                                                                                             
    {                                                                                                                                                                  
        mask = (1 << width) - 1;                                                                                                                                       
        temp = mipi_dsih_dphy_read_word(instance, reg_address);                                                                                                        
        temp &= ~(mask << shift);                                                                                                                                      
        temp |= (data & mask) << shift;                                                                                                                                
        mipi_dsih_dphy_write_word(instance, reg_address, temp);                                                                                                        
    }                                                                                                                                                                  
}                                                                                                                                                                      
uint32_t mipi_dsih_dphy_read_word(dphy_t * instance, uint32_t reg_address)                                                                                             
{                                                                                                                                                                      
    if (instance->core_read_function == 0)                                                                                                                             
    {                                                                                                                                                                  
        return ERR_DSI_INVALID_IO;                                                                                                                                     
    }                                                                                                                                                                  
    return instance->core_read_function(instance->address, reg_address);                                                                                               
}                                                                                                                                                                      
uint32_t mipi_dsih_dphy_read_part(dphy_t * instance, uint32_t reg_address, uint8_t shift, uint8_t width)                                                               
{                                                                                                                                                                      
    return (mipi_dsih_dphy_read_word(instance, reg_address) >> shift) & ((1 << width) - 1);                                                                            
}                                                                                                                                                                      
                                                                                                                                                                             
