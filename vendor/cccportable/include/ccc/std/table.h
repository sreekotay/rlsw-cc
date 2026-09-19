/*
 * Arena-backed row table: one generation of dense (key, value) rows plus a
 * maintained Find index (pow2 u32 probe). Map-shaped face for the 1-gen case.
 *
 * Prefer Table::[K,V] for new code. ArrayMap is the frozen predecessor with
 * the same core (cc_array_map_core_*); do not extend it.
 *
 * Sugar: Table::[K,V] / table_new::[K,V] / table_new_count::[K,V].
 * Concrete type is Table_<K>_<V>* (arena header — arrow UFCS).
 */
#ifndef CC_STD_TABLE_H
#define CC_STD_TABLE_H

#include "array_map.h"

#ifdef __CC_TABLE
#undef __CC_TABLE
#endif
#ifdef __CC_TABLE_INIT
#undef __CC_TABLE_INIT
#endif
#ifdef __CC_TABLE_INIT_COUNT
#undef __CC_TABLE_INIT_COUNT
#endif
#define __CC_TABLE(K, V) Table_##K##_##V
#define __CC_TABLE_INIT(K, V, arena) \
    Table_##K##_##V##_init(CC__ARENA_HANDLE(arena))
#define __CC_TABLE_INIT_COUNT(K, V, arena, count) \
    Table_##K##_##V##_init_count(CC__ARENA_HANDLE(arena), (count))

/* Same layout and core as ArrayMap; Name is Table_<K>_<V>. */
#define CC_TABLE_DECL(K, V, Name, HASH_FN, EQ_FN) \
    CC_ARRAY_MAP_DECL(K, V, Name, HASH_FN, EQ_FN)

#define Table(K, V, Name, HASH_FN, EQ_FN) \
    CC_TABLE_DECL(K, V, Name, HASH_FN, EQ_FN)

#define CC_TABLE_FOREACH(h, k_var, v_var) CC_ARRAY_MAP_FOREACH(h, k_var, v_var)

#define CC_TABLE_DECL_UFCS(Name) typedef char __cc_table_decl_ufcs__##Name

#if defined(CC_COMPTIME) || defined(__TINYC__)
#include <stdio.h>
#include <string.h>
#endif
                              
                                                  
                                            
                                                                        
                                                                        
                     
                              
              
                              
              
               
                   
                                                                   
                                    
                                                            
                                             
                        
     
                                                            
                                                                    
                                
                                                        
                                                  
                                              
                                                              
                                                        
                                                 
                                                                 
                                                           
                                                                          
                                                          
                                                    
                                                                          
                                                          
                                                    
                                         
                                                        
                                                  
                                                                         
                                                                       
                                                 
                                                      
                                                        
                                                  
            
                                                              
                                                        
                     
     
                 
                                  
                                                                   
                                    
                  
                           
                                                         
                                                         
                                       
 
     
      
                                                     
                                                     
                                
 
                                                       
                                                           
                                
 
                                                                   
                                                                  
                                           
 
                                                                 
      
          
 



#endif /* CC_STD_TABLE_H */
