#ifndef CC_STD_STATIC_MAP_CCH
#define CC_STD_STATIC_MAP_CCH

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ccc/cc_instantiate.h>
#include <ccc/cc_slice.h>

#ifndef CC_STATIC_MAP_TYPES_DEFINED
#define CC_STATIC_MAP_TYPES_DEFINED
enum {
    CC_STATIC_MAP_CASE_SENSITIVE = 0,
    CC_STATIC_MAP_ASCII_CI = 1,
};
#endif

/* Emit a frozen CCSlice -> value_type map at the comptime call site.
 *
 * The user-facing call is:
 *
 *   @comptime {
 *       RedisCommandEntry commands[] = {
 *           { "GET", { REDIS_CMD_GET, 1, 1, 0 } },
 *           { "SET", { REDIS_CMD_SET, 2, 2, 0 } },
 *       };
 *       static_map("redis_commands", commands, CC_STATIC_MAP_ASCII_CI);
 *   }
 *
 * `entries` is an array of a user struct with a `const char *key` field and a
 * real typed `value` field.  There is no stringified initializer: values are
 * ordinary typed compound literals the C front end checks.  The compiler
 * rewrites the three-argument call, inferring the value type from the array
 * element type and supplying the element/value layout via `sizeof` and address
 * arithmetic, into the internal form below.  The comptime function reads the
 * typed `.value` fields straight from the live comptime-evaluated array and
 * serializes each POD integer/enum field into a real typed initializer.
 *
 * Generated surface (unchanged in spirit):
 *
 *   static const value_type *<name>_get(CCSlice key);
 *
 * Lookup is a minimal perfect hash: hash(key) -> slot -> verify -> &value.
 * `flags` selects exact or ASCII case-insensitive matching.  Value fields must
 * be POD integers or enums; a field of any other kind (pointer, float, nested
 * aggregate) is a keyed comptime error, as is a value layout the model cannot
 * reproduce, a duplicate key, a key needing C-string escaping, or a failure to
 * construct a perfect hash.
 *
 * In a `.cch` header: lower_header blanks the `@comptime { }` block so the
 * `.h` stays host-C. Forward-declare `<name>_get` for in-header callers; the
 * including TU harvests and re-runs the block
 * (`cc_harvest_local_header_comptime_blocks`) so the definition is spliced
 * into the merged `.c`. Proof: `tests/comptime_static_map_in_header_smoke`. */
                                           
                                                 
                                              
                                       
                                        
                                         
                                           
                                            
                                      
                                                         
                                            
                
                          
                      
                  
                       
                         
                          
                   

                                                                           
                        
                   
                    
                   
                     

                                                             
                                                                                  
                                                       
               
     

                                                                             
                                                                             
                                                                              
                                                                           
     
                                                    
                       
                          
                                 
                                        
                                                                                   
                                 
                                
                   
         
                                          
                          
                          
                                                
                                                                                         
                                                                             

                                                                       
                    
                      
                                                    
                                                                                                       
                                                                                                          
                      
             

                                                                                                               
                                                                                                               
                                                                                                               
                                                                                                               
                                                                                                               
                                                                      
                                                                                                              
                                                                                                               
                                                                                                               
                                                                                                               
                                                                 
                                                                                                              
                                                                                                               
                                                                                                               
                                                                                                               
                                                                    
                                                                                                              
                                                                                                               
                                                                                                                    
                                                                                                               
                                                                                                               
                                                                                                               
                                                                                                               

                      
                                            
                                                                                               
                                         
                                    
                       
             
                                                             
                                
                           
                            
                              
                                               
         
                                                                       
                     
                                
                                        
                                                                                                   
                                                  
                                
                   
         
     

                                                                             
                                 
                             
                                                                     
                                                      
                                                                             
                   
         
                    
                                
                                 
                                                     
                         
                                                      
                                                            
                                                            
                               
                                                                                 
                                                                                 
                 
                                                
             
                       
                                                           
                       
             
         
     

                                                                            
                                                                 
                                       
                                         
                                                
                                                           
                       
                                         
                                  
                                                   
                                                                
                                                         
                                                           
                           
                                   
                 
                                                                       
                                                        
                                         
             
                                         
         
                                     
     
                 
                                                                  
               
     

                                            
                                                                     
                                 
                                                
                                      
     
                                            
              
                                                                    
                                 
                                                
                                       
     
                                            
              
                                                
                                 
                                 
              
                                                                            
                                       
                                     
                                                                                         
                                     
                                              
                           
                                         
                            
                                                            
                                                  
                                 
                                                                   
                                                
                    
                                                                    
                                                
             
         
                                                          
     
                                            
              
                                                    
                          
                                      
                                                
                                        
     
                                            
              
                                                   
                                 
                         
                                                                       
                                               
                                            
                    
                   
                                                
                                                                                       
     
                                            
                           
                                   
                 
                         
             
                                                            
                         
                                       
                                       
                                                           
                                                  
                                                        
                                                
                          
                   
                                                
                                                         
                                                                                     
                                                                                     
                                               
            
                                                
                                                     
     
                                            
                 
                         
             
                                                  
                                                             
                                              
                                         
                                                               
                                        
              
                                                         
                          
 

#endif
