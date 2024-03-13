    .global mc32nt4p_s
    .text
mc32nt4p_s:
    # x64 AVX(SSE)   
    #   4k ALIGNED DATA 
    #       It uses 2 channels memory architecture.
    #       prefetch by 128 bytes from page 1 & page 2 (prefetcht0)
    #       ymm0..1  - page 1, 64 bytes chunks (64 loops per page)
    #       ymm2..3 - page 2
    #       ymm4..5 - page 3
    #       ymm6..7 - page 4
    #
    #       non temporal store (vmovntdq)
    #
    # void mc32_s(uint8_t * restrict dst, const uint8_t * restrict src, size_t sz) __nonnull ((1, 2));
    # Entry:
    #     RDI: dst
    #     RSI: dst
    #     RDX: sz
    #
    #
    shr             $14, %rdx       # sz/16384 ()
    mov            %rdx, %rax       # return number of loops
    mov         $3*4096, %r8
    mov             $64, %r9
l1:
    mov             $64, %r10        #4096 byes/2 registers * 32 bytes each) = 64 loops/page
l2: # copy 4 x 4k pages
    prefetcht0  0*4096(%rsi)   # page 1
    prefetcht0  1*4096(%rsi)   # page 2
    prefetcht0  2*4096(%rsi)   # page 3
    prefetcht0  3*4096(%rsi)   # page 4
  
    vmovdqa  0+0*4096(%rsi), %ymm0 # load 64 bytes chunk from page 1 
    vmovdqa 32+0*4096(%rsi), %ymm1
    vmovdqa  0+1*4096(%rsi), %ymm2 # load 64 bytes chunk from page 2 
    vmovdqa 32+1*4096(%rsi), %ymm3
    vmovdqa  0+2*4096(%rsi), %ymm4 # load 64 bytes chunk from page 3 
    vmovdqa 32+2*4096(%rsi), %ymm5
    vmovdqa  0+3*4096(%rsi), %ymm6 # load 64 bytes chunk from page 4 
    vmovdqa 32+3*4096(%rsi), %ymm7

    vmovntdq %ymm0,  0+0*4096(%rdi) # store page's 1 chunk 
    vmovntdq %ymm1, 32+0*4096(%rdi)
    vmovntdq %ymm2,  0+1*4096(%rdi) # store page's 2 chunk 
    vmovntdq %ymm3, 32+1*4096(%rdi)
    vmovntdq %ymm4,  0+2*4096(%rdi) # store page's 3 chunk 
    vmovntdq %ymm5, 32+2*4096(%rdi)
    vmovntdq %ymm6,  0+3*4096(%rdi) # store page's 4 chunk 
    vmovntdq %ymm7, 32+3*4096(%rdi)
    
    add %r9, %rdi               # next chunk ; rsi += 64
    add %r9, %rsi               # next chunk ; rdi += 64
    dec %r10
    jnz l2
    add %r8, %rsi               # rsi += 3*4096
    add %r8, %rdi               # rdi += 3*4096
    dec %rdx                    # next 4  pages
#
#  at beginning of l1
# rsi -> +----------+
#        |  page 1  |
#        +----------+ (1*4096)%rsi
#        |  page 2  |
#        +----------+ (2*4096)%rsi
#        |  page 3  |
#        +----------+ (3*4096)%rsi
#        |  page 4  |
#        +----------+
#
#  after end of l1
#        +----------+
#        |  page 1  |
# rsi -> +----------+ (4096)%rsi
#        |  page 2  |
#        +----------+ (2*4096)%rsi
#        |  page 3  |
#        +----------+ (3*4096)%rsi
#        |  page 4  |
#        +----------+

    jnz l1
    sfence
    ret

