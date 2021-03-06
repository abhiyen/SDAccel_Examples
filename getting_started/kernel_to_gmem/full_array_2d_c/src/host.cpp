/**********
Copyright (c) 2017, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#include <iostream>
#include <cstring>

//OpenCL utility layer include
#include "xcl.h"

//define array size to access
#define DATA_SIZE 8
//define max local buffer size
#define N 256

void mmult_sw(int *a, int *b, int *c, int size)
{
  int bufa[N][N], bufb[N][N], bufc[N][N];
  for (int i = 0 ; i < size ; i++) {
      memcpy(bufa[i], (int *) &a[i*size], size*sizeof(int));
      memcpy(bufb[i], (int *) &b[i*size], size*sizeof(int));
  }

  for (int row = 0; row < size; row++) {
    for (int col = 0; col < size; col++) {
      int result = 0;
      for (int k = 0; k < size; k++) {
        result += bufa[row][k] * bufb[k][col];
      }
      bufc[row][col] = result;
    }
  }
  for (int i = 0 ; i < size ; i++)
      memcpy((int *) &c[i*size], bufc[i], size*sizeof(int));
}

int main(int argc, char** argv)
{
    int size = DATA_SIZE;
    int matrix_size = size * size;
    if (size > N) {
        std::cout << "Size is bigger than internal buffer size, please use a size smaller than 256!" << std::endl;
        return EXIT_FAILURE;
    }
    //Allocate Memory in Host Memory
    size_t vector_size_bytes = sizeof(int) * matrix_size;

    int *source_input_a     = (int *) malloc(vector_size_bytes);
    int *source_input_b     = (int *) malloc(vector_size_bytes);
    int *source_hw_results  = (int *) malloc(vector_size_bytes);
    int *source_sw_results  = (int *) malloc(vector_size_bytes);

    // Create the test data and Software Result 
    for(int i = 0 ; i < matrix_size ; i++){
        source_input_a[i] = i;
        source_input_b[i] = i;
        source_hw_results[i] = 0;
    }
    mmult_sw(source_input_a, source_input_b, source_sw_results, size);

//OPENCL HOST CODE AREA START
    //Create Program and Kernel
    xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "mmult");
    cl_kernel krnl_mmult = xcl_get_kernel(program, "mmult");

    //Allocate Buffer in Global Memory
    cl_mem buffer_a = xcl_malloc(world, CL_MEM_READ_ONLY, vector_size_bytes);
    cl_mem buffer_b = xcl_malloc(world, CL_MEM_READ_ONLY, vector_size_bytes);
    cl_mem buffer_c = xcl_malloc(world, CL_MEM_WRITE_ONLY, vector_size_bytes);

    //Copy input data to device global memory
    xcl_memcpy_to_device(world,buffer_a,source_input_a,vector_size_bytes);
    xcl_memcpy_to_device(world,buffer_b,source_input_b,vector_size_bytes);

    //Set the Kernel Arguments
    xcl_set_kernel_arg(krnl_mmult,0,sizeof(cl_mem),&buffer_a);
    xcl_set_kernel_arg(krnl_mmult,1,sizeof(cl_mem),&buffer_b);
    xcl_set_kernel_arg(krnl_mmult,2,sizeof(cl_mem),&buffer_c);
    xcl_set_kernel_arg(krnl_mmult,3,sizeof(int),&size);

    //Launch the Kernel
    xcl_run_kernel3d(world,krnl_mmult,1,1,1);

    //Copy Result from Device Global Memory to Host Local Memory
    xcl_memcpy_from_device(world, source_hw_results, buffer_c ,vector_size_bytes);
    clFinish(world.command_queue);

    //Release Device Memories and Kernels
    clReleaseMemObject(buffer_a);
    clReleaseMemObject(buffer_b);
    clReleaseMemObject(buffer_c);
    clReleaseKernel(krnl_mmult);
    clReleaseProgram(program);
    xcl_release_world(world);
//OPENCL HOST CODE AREA END
    
    // Compare the results of the Device to the simulation
    int match = 0;
    std::cout << "Result = " << std::endl;
    for (int i = 0 ; i < matrix_size ; i++){
        if (source_hw_results[i] != source_sw_results[i]){
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                << " Device result = " << source_hw_results[i] << std::endl;
            match = 1;
            break;
        }else{
            std::cout << source_hw_results[i] << " " ;
            if ( ( (i+1) % 16) == 0) std::cout << std::endl;
        }
    }

    // Release Memory from Host Memory
    free(source_input_a);
    free(source_input_b);
    free(source_hw_results);
    free(source_sw_results);

    if (match){
        std::cout << "TEST FAILED." << std::endl; 
        return EXIT_FAILURE;
    }
    std::cout << "TEST PASSED." << std::endl; 
    return EXIT_SUCCESS; 
}
