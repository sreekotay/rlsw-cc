/*
 * BufReader::[Src] — generic buffered reader over Duplex-compatible sources.
 *
 * Fill dispatches with _Generic on Src* to the duplex caller-buffer read
 * (EOF model B). Extend the association list when adding a new duplex type.
 *
 * Factory bodies are harvested from this `.cch` (blanked from the lowered `.h`).
 */
#ifndef CC_STD_BUFIO_H
#define CC_STD_BUFIO_H

/* Loud fallback when Src is not in the fill _Generic list. */
static inline CCResult_bool_CCIoError cc__bufio_unsupported_read(void *src, char *buf,
                                                                 size_t max, size_t *out) {
    (void)src; (void)buf; (void)max;
    if (out) *out = 0;
    return cc_err_CCResult_bool_CCIoError(cc_io_error_os(CC_IO_INVALID_ARGUMENT, EINVAL));
}

                                  
                  
                           
                                   
                               
      
                
                   
              
                         
            
             
                                                                                          
                   
                             
                                             
 
                                                                                               
              
                                                              
                                            
                        
                                      
             
 
                                               
                                                                   
                                                               
                                                           
 
                                                                 
                        
                          
                                            
                                                
                                    
                                        
                                                          
                                     
                                                            
                            
                                                        
                           
                   
                                            
      
                                        
                             
                              
      
                                           
                                          
                                 
                                                           
                
                        
 
                                                                                    
                                                          
                                        
                                                   
                                                       
                          
                                                                      
     
                                                                        
                        
 
                                                                         
             
                                                    
              
                                           
                                        
                                                                  
                                                                      
                                                              
                               
                                
             
         
                                                  
                                                     
                                                         
                              
                                                           
                                                                        
             
     
 
          
 

                                         
                  
                                                                                        
                                                          
                                                                            
                                 
                                 
                        
                             
                           
                                              
                                                                   
                                            
 
                                        
                                                        
          
 

#endif /* CC_STD_BUFIO_H */
