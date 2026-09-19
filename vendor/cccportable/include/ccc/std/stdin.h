/*
 * Stdin as a Duplex-compatible source + BufReader::[CCStdin] script helpers.
 *
 * Script predecl (token `stdin`): a BufReader value named `stdin` over fd 0.
 *   bool more = stdin.read_line(&line) !>;
 *   char[:] input = stdin.read_all(arena) !>;
 *
 * Uses read(2) on fd 0 so the BufReader binding does not collide with libc's
 * FILE *stdin when shadowed by a local named `stdin`.
 */
#ifndef CC_STD_STDIN_H
#define CC_STD_STDIN_H

#include <errno.h>
#include <unistd.h>

#include <ccc/cc_io_error.h>
#include <ccc/cc_result.h>
#include <ccc/cc_slice.h>
#include <ccc/std/string.h>

typedef struct CCStdin {
    CCArena line_arena;
} CCStdin;

static inline void cc_stdin_init(CCStdin *src, CCArena arena) {
    if (src) src->line_arena = arena;
}

static inline CCResult_bool_CCIoError cc_stdin_read_buf_into(CCStdin *src, char *buf,
                                                         size_t max, size_t *out) {
    ssize_t n;
    (void)src;
    if (!buf || !out || !max) {
        return cc_err_CCResult_bool_CCIoError(cc_io_error(CC_IO_INVALID_ARGUMENT));
    }
    for (;;) {
        n = read(0, buf, max);
        if (n < 0) {
            if (errno == EINTR) continue;
            return cc_err_CCResult_bool_CCIoError(cc_io_from_errno(errno));
        }
        break;
    }
    if (n == 0) {
        *out = 0;
        return cc_ok_CCResult_bool_CCIoError(false);
    }
    *out = (size_t)n;
    return cc_ok_CCResult_bool_CCIoError(true);
}

#define CC_BUFIO_EXTRA_GENERIC CCStdin *: cc_stdin_read_buf_into,
#include <ccc/std/bufio.h>

                                         
                                                                         
                  
                                                                            
                                           
                             
 
                                                                                        
                                    
                
                  
                                  
                                                                       
     
                                
                                   
                                                                       
     
              
                        
                  
                                                             
                      
                                                  
                                                                 
                
                               
                        
                                             
                                                                                             
             
                              
         
                
                                          
                                     
                             
                                
                                                   
                                                                 
                                                                                  
             
                  
         
                                                        
                                                                          
     
                                                               
                                                 
                                                            
                                                                        
                       
 
                                                                                      
                                           
                
                                        
                                                                      
                                                        
                                                                             
              
                                                             
                      
                                                     
                                               
                                                                          
                                                                                          
                                                          
                                                     
             
                                                                          
                                 
                              
                     
         
                                                                
                      
                                           
                                                                                           
           
                            
                                                     
                                             
                                                                        
                                                                                        
                                                        
                                                   
           
                                                            
                                   
     
                                                                               
 
                                    
                                                    
          
 

/* `read_line(&line)` arity lives in postlude.cch — not this decl header. */

#endif /* CC_STD_STDIN_H */
