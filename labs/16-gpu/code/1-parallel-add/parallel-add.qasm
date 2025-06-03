.include "../share/vc4inc/vc4.qinc"

# LOAD UNIFORMS INTO REGISTERS  
mov   ra0, unif #A
mov   ra1, unif #B
mov   ra2, unif #C
mov   ra3, unif # @joshdelg N / 16 -> num rows

mov   rb0, 64 # ROW WIDTH


# YOU WILL PROBABLY NEED A LOOP OF SOME SORT
:loop

    # DMA READ A AND B FROM PHYSICAL MEMORY TO VPM
    # Since we are only using one QMU, can only do 16 elements at once
    # So, we load one row at a time (one row of 16 elements which are 4 bytes each)

    # DMA READ A
    mov vr_setup, vdr_setup_0(1, 16, 1, vdr_h32(1, 0, 0))
    # mov vr_setup, vdr_setup_0(1, 16, 2, vdr_h32(1, 0, 0)) # Read 2 rows
                                # How will you do the read?
    mov vr_addr, ra0            # USE YOUR UNIFORM ADDRESS FOR A 
                                # (will probably need to change with the loop)
    mov -, vr_wait              # KICK OFF THE READ

    # REPEAT FOR B
    mov vr_setup, vdr_setup_0(1, 16, 1, vdr_h32(1, 1, 0)) # COuld I merge this with the last one?
                                # How will you do the read?
    mov vr_addr, ra1            # USE YOUR UNIFORM ADDRESS FOR B
                                # (will probably need to change with the loop)
    mov -, vr_wait              # KICK OFF THE READ

    # SETUP VPM READS/WRITES (the vpm_setup macro to configure read/write) 
    mov vr_setup, vpm_setup(2, 1, h32(0)) # Num = 2 because going to read Row 0 (A) and Row 1 (B)
    mov vw_setup, vpm_setup(1, 1, h32(2)) # Let's write the add result into row index 2

    # READ FROM VPM INTO REGISTERS

    mov ra4, vpm
    mov -, vw_wait        # WE HAVE TO DO THIS FOR VPM READS/WRITES

    mov rb1, vpm
    mov -, vw_wait        # WE HAVE TO DO THIS FOR VPM READS/WRITES

    mov r1, ra4
    mov r2, rb1

    # TODO: DO THE ADD
    # ! Is this bad? Could load vpm into raX and rbX, then mov to accumulators, then add
    add ra4, r1, r2

    nop
    nop

    # TODO: WRITE IT BACK OUT TO VPM (write already configured)
    mov vpm, ra4
    mov -, vw_wait        # WE HAVE TO DO THIS FOR VPM READS/WRITES

    # TODO: DMA WRITE FROM VPM TO PHYSICAL MEMORY
    mov vw_setup, vdw_setup_0(1, 16, dma_h32(2, 0)) # USE vdw_setup_0 MACRO TO 
                                              # DEFINE HOW YOU'LL DO THE READ
    mov vw_addr, ra2            # USE YOUR UNIFORM ADDRESS FOR C
    mov -, vw_wait              # KICK OFF THE WRITE


    
    # FIGURE OUT WHETHER TO CONTINUE THE LOOP
    # THE FOLLOWING IS EQUIVALENT TO: for (int i=<initial ra3 value>, i > 0; i--)

    # Add 64 (16 * 4) to mem addrs A, B, C
    mov r1, rb0

    mov r0, ra0
    add r0, r0, r1
    mov ra0, r0

    nop
    nop

    mov r0, ra1
    add r0, r0, r1
    mov ra1, r0

    nop
    nop

    mov r0, ra2
    add r0, r0, r1
    mov ra2, r0

    nop
    nop

    sub.setf ra3, ra3, 1
    brr.anynz -, :loop

    # DO NOT REMOVE DO NOT REMOVE DO NOT REMOVE
    # After a branch, the 3 subsequent instructions are executed
    # If you want to do something useful with these 3 instructions in your kernel, feel free
    nop
    nop
    nop
    # END DO NOT REMOVE 


# End of kernel
:end
thrend
mov interrupt, 1
nop
nop
