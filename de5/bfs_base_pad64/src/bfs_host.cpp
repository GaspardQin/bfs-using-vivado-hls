//-----------------------------------------------------------------------------
// Filename: bfs_host.cpp
// Version: 1.0
// Description: Breadth-first search OpenCL benchmark.
//
// Author:      Cheng Liu
// Email:       liucheng@ict.ac.cn, st.liucheng@gmail.com
// Affiliation: Institute of Computing Technology, Chinese Academy of Sciences
//
// Acknowledgement:
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <malloc.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

#include "graph.h"
#include "opencl_utils.h"

#define SW_EMU
#define GROUP 4
#ifdef SW_EMU
#define BMAP_DEPTH (16 * 1024) 
#endif

#define HERE do {std::cout << "File: " << __FILE__ << " Line: " << __LINE__ << std::endl;} while(0)

#define ERROR(FMT, ARG...) do {fprintf(stderr,"File=%s, Line=%d  \
        " FMT " \n",__FILE__, __LINE__, ##ARG); exit(-1);} while(0)

#define PRINT(FMT, ARG...) do {fprintf(stdout,"File=%s, Line=%d  \
        " FMT " \n",__FILE__, __LINE__, ##ARG);} while(0)

#define AOCL_ALIGNMENT 64

template<class T>
struct AlignedArray{
	AlignedArray(size_t numElts){ data = (T*) memalign( AOCL_ALIGNMENT, sizeof(T) * numElts ); }
	~AlignedArray(){ free(data); }

	T& operator[](size_t idx){ return data[idx]; }
	const T& operator[](size_t idx) const { return data[idx]; }

	T* data;
};

Graph* createGraph(const std::string &gName){
	std::string dir = "/home/liucheng/graph-data/";
	std::string fName;

	if     (gName == "dblp")        fName = "dblp.ungraph.txt";
	else if(gName == "youtube")     fName = "youtube.ungraph.txt";
	else if(gName == "lj")          fName = "lj.ungraph.txt";
	else if(gName == "pokec")       fName = "pokec-relationships.txt";
	else if(gName == "wiki-talk")   fName = "wiki-Talk.txt";
	else if(gName == "lj1")         fName = "LiveJournal1.txt";
	else if(gName == "orkut")       fName = "orkut.ungraph.txt";
	else if(gName == "rmat-21-32")  fName = "rmat-21-32.txt";
	else if(gName == "rmat-21-64")  fName = "rmat-19-32.txt";
	else if(gName == "rmat-21-128") fName = "rmat-21-128.txt";
	else if(gName == "twitter")     fName = "twitter_rv.txt";
	else if(gName == "friendster")  fName = "friendster.ungraph.txt";
	else ERROR(" Unknown graph name %s .", gName.c_str());

	std::string fpath = dir + fName;
	return new Graph(fpath.c_str());
}

void swBfsInit(int vertexNum, char* depth, const int &vertexIdx){
	for(int i = 0; i < vertexNum; i++){
		depth[i] = -1;
	}
	depth[vertexIdx] = 0;
}

void swBfs(CSR* csr, char* depth, const int &vertexIdx){
	std::vector<int> frontier;
	char level = 0;
	while(true){
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				int start = csr->rpao[2 * i];
				int num   = csr->rpao[2 * i + 1];
				for(int cidx = 0; cidx < num; cidx++){
					int ongb = csr->ciao[start + cidx];
					if(ongb != -1){
						if(depth[ongb] == -1){
							depth[ongb] = level + 1;
						}
					}
				}
			}
		}

		if(frontier.empty()){
			break;
		}
		std::cout << "swBfs iteration: " << (int)level  << ", frontier size: " << frontier.size() << std::endl;

		level++;
		frontier.clear();
	}
}

void hwBfsInit(int vertexNum, char* depth, int rootVidx){
	for(int i = 0; i < vertexNum; i++){
		if(i == rootVidx){
			depth[i] = 0;
		}
		else{
			depth[i] = -1;
		}
	}
}

int verify(char* swDepth, char* hwDepth, const int &num){
	bool match = true;
	for (int i = 0; i < num; i++) {
		if (swDepth[i] != hwDepth[i]) {
			PRINT("swDepth[%d] = %d, hwDepth[%d] = %d\n", i, swDepth[i], i, hwDepth[i]);	
			match = false;
			break;
		} 
	}

	if (match){
		printf("TEST PASSED.\n");
		return EXIT_SUCCESS;
	} 
	else{
		printf("TEST FAILED.\n");
		return -1;
	}
}

int getStartVertexIdx(const std::string &gName){
	if(gName == "youtube")    return 320872;
	if(gName == "lj1")        return 3928512;
	if(gName == "pokec")      return 182045;
	if(gName == "rmat-19-32") return 104802;
	if(gName == "rmat-21-32") return 365723;
	return -1;
}

// Sum the array
int getSum(int *ptr, int num){	
	if(ptr == nullptr) return -1;
	int sum = 0;
	for(int i = 0; i < num; i++){
		sum += ptr[i];
	}
	return sum;
}

int align(int num, int dataWidth, int alignedWidth){
	if(dataWidth > alignedWidth){
		std::cout << "Aligning to smaller data width is not supported." << std::endl;
		return -1;
	}
	else{
		int wordNum = alignedWidth / dataWidth;
		int alignedNum = ((num - 1)/wordNum + 1) * wordNum;
		return alignedNum;
	}
}

void addKernelNames(std::vector<std::string> &kernelNames){
	kernelNames.push_back("read_rpa");
	kernelNames.push_back("read_cia");
	kernelNames.push_back("traverse_cia0");
	kernelNames.push_back("traverse_cia1");
	kernelNames.push_back("traverse_cia2");
	kernelNames.push_back("traverse_cia3");
	kernelNames.push_back("write_frontier0to3");
	kernelNames.push_back("write_frontier4to7");
	kernelNames.push_back("write_frontier8to11");
	kernelNames.push_back("write_frontier12to15");
	kernelNames.push_back("write_frontier16to19");
	kernelNames.push_back("write_frontier20to23");
	kernelNames.push_back("write_frontier24to27");
	kernelNames.push_back("write_frontier28to31");
	kernelNames.push_back("write_frontier32to35");
	kernelNames.push_back("write_frontier36to39");
	kernelNames.push_back("write_frontier40to43");
	kernelNames.push_back("write_frontier44to47");
	kernelNames.push_back("write_frontier48to51");
	kernelNames.push_back("write_frontier52to55");
	kernelNames.push_back("write_frontier56to59");
	kernelNames.push_back("write_frontier60to63");
}

void initRpaInfo(AlignedArray<int> &rpaInfo, CSR* csr, const int &rootVidx){
	rpaInfo[0] = csr->rpao[2 * rootVidx];
	rpaInfo[1] = csr->rpao[2 * rootVidx + 1];
}

int main(int argc, char ** argv){

	spector::ClContext clContext;
	cl_device_type deviceType = CL_DEVICE_TYPE_ACCELERATOR;	
	std::vector<std::string> kernelNames;
	addKernelNames(kernelNames);
	const char* clFileName   = "bfs_fpga.cl";
	const char* aocxFileName = "bfs_fpga.aocx";
	if(!init_opencl(&clContext, kernelNames, deviceType, clFileName, aocxFileName)){exit(-1);}
	int         pad   = 64;
	std::string gName = "youtube";
	Graph* gptr = createGraph(gName);
	int millionEdges = gptr->edgeNum/(1000000.0);
	CSR* csr = new CSR(*gptr, pad);
	free(gptr);
	int vertexNum = csr->vertexNum; 
	int rpaoSize = (int)(csr->rpao.size());
	int ciaoSize = (int)(csr->ciao.size());
	int segSize  = (vertexNum + pad - 1) / pad;
	int rootVidx = getStartVertexIdx(gName);
	std::cout << "Graph is loaded." << std::endl;
	std::cout << "rpaoSize: " << rpaoSize << std::endl;
	std::cout << "ciaoSize: " << ciaoSize << std::endl;
	char* hwDepth = (char*)malloc(vertexNum * sizeof(char));
	char* swDepth = (char*)malloc(vertexNum * sizeof(char));
	AlignedArray<int>  rpaInfo(2 * vertexNum);
	AlignedArray<int>  cia(ciaoSize);
	for(int i = 0; i < ciaoSize; i++){
		cia[i] = csr->ciao[i];
	}
	AlignedArray<int> nextFrontier0(segSize);
	AlignedArray<int> nextFrontier1(segSize);
	AlignedArray<int> nextFrontier2(segSize);
	AlignedArray<int> nextFrontier3(segSize);
	AlignedArray<int> nextFrontier4(segSize);
	AlignedArray<int> nextFrontier5(segSize);
	AlignedArray<int> nextFrontier6(segSize);
	AlignedArray<int> nextFrontier7(segSize);

	AlignedArray<int> nextFrontier8(segSize);
	AlignedArray<int> nextFrontier9(segSize);
	AlignedArray<int> nextFrontier10(segSize);
	AlignedArray<int> nextFrontier11(segSize);
	AlignedArray<int> nextFrontier12(segSize);
	AlignedArray<int> nextFrontier13(segSize);
	AlignedArray<int> nextFrontier14(segSize);
	AlignedArray<int> nextFrontier15(segSize);

	AlignedArray<int> nextFrontier16(segSize);
	AlignedArray<int> nextFrontier17(segSize);
	AlignedArray<int> nextFrontier18(segSize);
	AlignedArray<int> nextFrontier19(segSize);
	AlignedArray<int> nextFrontier20(segSize);
	AlignedArray<int> nextFrontier21(segSize);
	AlignedArray<int> nextFrontier22(segSize);
	AlignedArray<int> nextFrontier23(segSize);

	AlignedArray<int> nextFrontier24(segSize);
	AlignedArray<int> nextFrontier25(segSize);
	AlignedArray<int> nextFrontier26(segSize);
	AlignedArray<int> nextFrontier27(segSize);
	AlignedArray<int> nextFrontier28(segSize);
	AlignedArray<int> nextFrontier29(segSize);
	AlignedArray<int> nextFrontier30(segSize);
	AlignedArray<int> nextFrontier31(segSize);

	AlignedArray<int> nextFrontier32(segSize);
	AlignedArray<int> nextFrontier33(segSize);
	AlignedArray<int> nextFrontier34(segSize);
	AlignedArray<int> nextFrontier35(segSize);
	AlignedArray<int> nextFrontier36(segSize);
	AlignedArray<int> nextFrontier37(segSize);
	AlignedArray<int> nextFrontier38(segSize);
	AlignedArray<int> nextFrontier39(segSize);

	AlignedArray<int> nextFrontier40(segSize);
	AlignedArray<int> nextFrontier41(segSize);
	AlignedArray<int> nextFrontier42(segSize);
	AlignedArray<int> nextFrontier43(segSize);
	AlignedArray<int> nextFrontier44(segSize);
	AlignedArray<int> nextFrontier45(segSize);
	AlignedArray<int> nextFrontier46(segSize);
	AlignedArray<int> nextFrontier47(segSize);

	AlignedArray<int> nextFrontier48(segSize);
	AlignedArray<int> nextFrontier49(segSize);
	AlignedArray<int> nextFrontier50(segSize);
	AlignedArray<int> nextFrontier51(segSize);
	AlignedArray<int> nextFrontier52(segSize);
	AlignedArray<int> nextFrontier53(segSize);
	AlignedArray<int> nextFrontier54(segSize);
	AlignedArray<int> nextFrontier55(segSize);

	AlignedArray<int> nextFrontier56(segSize);
	AlignedArray<int> nextFrontier57(segSize);
	AlignedArray<int> nextFrontier58(segSize);
	AlignedArray<int> nextFrontier59(segSize);
	AlignedArray<int> nextFrontier60(segSize);
	AlignedArray<int> nextFrontier61(segSize);
	AlignedArray<int> nextFrontier62(segSize);
	AlignedArray<int> nextFrontier63(segSize);

	AlignedArray<int> nextFrontierSize(pad);
	std::vector<int*> nextFrontier(pad);
	nextFrontier[0] = nextFrontier0.data;
	nextFrontier[1] = nextFrontier1.data;
	nextFrontier[2] = nextFrontier2.data;
	nextFrontier[3] = nextFrontier3.data;
	nextFrontier[4] = nextFrontier4.data;
	nextFrontier[5] = nextFrontier5.data;
	nextFrontier[6] = nextFrontier6.data;
	nextFrontier[7] = nextFrontier7.data;

	nextFrontier[8]  = nextFrontier8.data;
	nextFrontier[9]  = nextFrontier9.data;
	nextFrontier[10] = nextFrontier10.data;
	nextFrontier[11] = nextFrontier11.data;
	nextFrontier[12] = nextFrontier12.data;
	nextFrontier[13] = nextFrontier13.data;
	nextFrontier[14] = nextFrontier14.data;
	nextFrontier[15] = nextFrontier15.data;

	nextFrontier[16] = nextFrontier16.data;
	nextFrontier[17] = nextFrontier17.data;
	nextFrontier[18] = nextFrontier18.data;
	nextFrontier[19] = nextFrontier19.data;
	nextFrontier[20] = nextFrontier20.data;
	nextFrontier[21] = nextFrontier21.data;
	nextFrontier[22] = nextFrontier22.data;
	nextFrontier[23] = nextFrontier23.data;

	nextFrontier[24] = nextFrontier24.data;
	nextFrontier[25] = nextFrontier25.data;
	nextFrontier[26] = nextFrontier26.data;
	nextFrontier[27] = nextFrontier27.data;
	nextFrontier[28] = nextFrontier28.data;
	nextFrontier[29] = nextFrontier29.data;
	nextFrontier[30] = nextFrontier30.data;
	nextFrontier[31] = nextFrontier31.data;

	nextFrontier[32] = nextFrontier32.data;
	nextFrontier[33] = nextFrontier33.data;
	nextFrontier[34] = nextFrontier34.data;
	nextFrontier[35] = nextFrontier35.data;
	nextFrontier[36] = nextFrontier36.data;
	nextFrontier[37] = nextFrontier37.data;
	nextFrontier[38] = nextFrontier38.data;
	nextFrontier[39] = nextFrontier39.data;
                                     
	nextFrontier[40] = nextFrontier40.data;
	nextFrontier[41] = nextFrontier41.data;
	nextFrontier[42] = nextFrontier42.data;
	nextFrontier[43] = nextFrontier43.data;
	nextFrontier[44] = nextFrontier44.data;
	nextFrontier[45] = nextFrontier45.data;
	nextFrontier[46] = nextFrontier46.data;
	nextFrontier[47] = nextFrontier47.data;
                                     
	nextFrontier[48] = nextFrontier48.data;
	nextFrontier[49] = nextFrontier49.data;
	nextFrontier[50] = nextFrontier50.data;
	nextFrontier[51] = nextFrontier51.data;
	nextFrontier[52] = nextFrontier52.data;
	nextFrontier[53] = nextFrontier53.data;
	nextFrontier[54] = nextFrontier54.data;
	nextFrontier[55] = nextFrontier55.data;
                                     
	nextFrontier[56] = nextFrontier56.data;
	nextFrontier[57] = nextFrontier57.data;
	nextFrontier[58] = nextFrontier58.data;
	nextFrontier[59] = nextFrontier59.data;
	nextFrontier[60] = nextFrontier60.data;
	nextFrontier[61] = nextFrontier61.data;
	nextFrontier[62] = nextFrontier62.data;
	nextFrontier[63] = nextFrontier63.data;

	//----------------------
	// software bfs
	//----------------------
	std::cout << "soft bfs starts." << std::endl;
	swBfsInit(vertexNum, swDepth, rootVidx);
	auto begin = std::chrono::high_resolution_clock::now();
	swBfs(csr, swDepth, rootVidx);
	auto end = std::chrono::high_resolution_clock::now();
	double elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	std::cout << "Software bfs takes " << elapsedTime << " ms." << std::endl;
	std::cout << "Measured MTEPS: " << 1000 * millionEdges/elapsedTime << std::endl;

	//--------------------------------
	// hardware accelerated bfs
	//--------------------------------
	initRpaInfo(rpaInfo, csr, rootVidx);
	hwBfsInit(vertexNum, hwDepth, rootVidx);
	char level       = 0;
	int frontierSize = 1;

	// Note that some of the queues may be reused in different execution steps
	cl_context context             = clContext.context;
	cl_command_queue readRpaQueue  = clContext.queues[0];
	cl_command_queue readCiaQueue  = clContext.queues[1];
	cl_command_queue TravCiaQueue0 = clContext.queues[2];
	cl_command_queue TravCiaQueue1 = clContext.queues[3];
	cl_command_queue TravCiaQueue2 = clContext.queues[4];
	cl_command_queue TravCiaQueue3 = clContext.queues[5];
	std::vector<cl_command_queue> writeQueues(pad/GROUP);
	for(int i = 0; i < pad/GROUP; i++){
		writeQueues[i]  = clContext.queues[i + 6];
	}
	cl_kernel* kernel              = clContext.kernels.data();
	cl_device_id deviceId          = clContext.device;
	
	cl_int err;
	cl_mem devRpa = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int) * 2 * vertexNum, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");
	cl_mem devCia = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int) * ciaoSize, NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");
	std::vector<cl_mem> devNextFrontier(pad);
	for(int i = 0; i < pad; i++){
		devNextFrontier[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * segSize, NULL, &err);
		spector::checkErr(err, "Failed to allocate memory!");
	}
	cl_mem devNextFrontierSize = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * pad,  NULL, &err);
	spector::checkErr(err, "Failed to allocate memory!");

#ifdef SW_EMU
	std::vector<cl_mem> devBitmap(pad);
	for(int i = 0; i < pad; i++){
		devBitmap[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * BMAP_DEPTH, NULL, &err);
		spector::checkErr(err, "Failed to allocate memory!");
	}
#endif

	err = clEnqueueWriteBuffer(readRpaQueue, devRpa, CL_TRUE, 0, sizeof(int) * frontierSize * 2, rpaInfo.data, 0, NULL, NULL);
	spector::checkErr(err, "Failed to write memory!");
	err = clEnqueueWriteBuffer(readCiaQueue, devCia, CL_TRUE, 0, sizeof(int) * ciaoSize, cia.data, 0, NULL, NULL);
	spector::checkErr(err, "Failed to write memory!");
	clFinish(readRpaQueue);
	clFinish(readCiaQueue);

	HERE;
	//clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void*)&devRpa);
	//clSetKernelArg(kernel[0], 1, sizeof(int),    (void*)&frontierSize);
	clSetKernelArg(kernel[1], 0, sizeof(cl_mem), (void*)&devCia);
	//clSetKernelArg(kernel[1], 1, sizeof(int),    (void*)&frontierSize);
	HERE;
	clSetKernelArg(kernel[2], 0, sizeof(cl_mem), (void*)&devNextFrontierSize);
	clSetKernelArg(kernel[2], 1, sizeof(int),    (void*)&rootVidx);
	clSetKernelArg(kernel[3], 0, sizeof(cl_mem), (void*)&devNextFrontierSize);
	clSetKernelArg(kernel[3], 1, sizeof(int),    (void*)&rootVidx);
	clSetKernelArg(kernel[4], 0, sizeof(cl_mem), (void*)&devNextFrontierSize);
	clSetKernelArg(kernel[4], 1, sizeof(int),    (void*)&rootVidx);
	clSetKernelArg(kernel[5], 0, sizeof(cl_mem), (void*)&devNextFrontierSize);
	clSetKernelArg(kernel[5], 1, sizeof(int),    (void*)&rootVidx);

	HERE;
#ifdef SW_EMU
	for(int i = 0; i < 16; i++){
		clSetKernelArg(kernel[2], 3 + i, sizeof(cl_mem), (void*)&devBitmap[i]);
		clSetKernelArg(kernel[3], 3 + i, sizeof(cl_mem), (void*)&devBitmap[16 + i]);
		clSetKernelArg(kernel[4], 3 + i, sizeof(cl_mem), (void*)&devBitmap[32 + i]);
		clSetKernelArg(kernel[5], 3 + i, sizeof(cl_mem), (void*)&devBitmap[48 + i]);
	}
#endif

	for(int i = 0; i < pad/GROUP; i++){
		clSetKernelArg(kernel[6 + i],  0, sizeof(cl_mem), (void*)&(devNextFrontier[GROUP * i]));
		clSetKernelArg(kernel[6 + i],  1, sizeof(cl_mem), (void*)&(devNextFrontier[GROUP * i + 1]));
		clSetKernelArg(kernel[6 + i],  2, sizeof(cl_mem), (void*)&(devNextFrontier[GROUP * i + 2]));
		clSetKernelArg(kernel[6 + i],  3, sizeof(cl_mem), (void*)&(devNextFrontier[GROUP * i + 3]));
	}

	HERE;
	std::vector<double> fpgaTime;
	begin = std::chrono::high_resolution_clock::now();
	while(frontierSize > 0){
		clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void*)&devRpa);
		clSetKernelArg(kernel[0], 1, sizeof(int),    (void*)&frontierSize);
		clSetKernelArg(kernel[1], 1, sizeof(int),    (void*)&frontierSize);
		clSetKernelArg(kernel[2], 2, sizeof(char),   (void*)&level);
		clSetKernelArg(kernel[3], 2, sizeof(char),   (void*)&level);
		clSetKernelArg(kernel[4], 2, sizeof(char),   (void*)&level);
		clSetKernelArg(kernel[5], 2, sizeof(char),   (void*)&level);

		HERE;
		auto t0 = std::chrono::high_resolution_clock::now();
		err = clEnqueueTask(readRpaQueue, kernel[0],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(readCiaQueue, kernel[1],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(TravCiaQueue0, kernel[2],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(TravCiaQueue1, kernel[3],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(TravCiaQueue2, kernel[4],  0, NULL, NULL);
		spector::checkErr(err, "Failed to enqueue task!");
		err = clEnqueueTask(TravCiaQueue3, kernel[5],  0, NULL, NULL);

		spector::checkErr(err, "Failed to enqueue task!");
		for(int i = 0; i < pad/GROUP; i++){
			err = clEnqueueTask(writeQueues[i], kernel[6 + i],  0, NULL, NULL);
			spector::checkErr(err, "Failed to enqueue task!");
		}
	
		HERE;
		clFinish(readRpaQueue);
		clFinish(readCiaQueue);
		clFinish(TravCiaQueue0);
		clFinish(TravCiaQueue1);
		clFinish(TravCiaQueue2);
		clFinish(TravCiaQueue3);
		HERE;
		for(int i = 0; i < pad/GROUP; i++){
			clFinish(writeQueues[i]);
		}
		HERE;
		auto t1 = std::chrono::high_resolution_clock::now();
		double t = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		fpgaTime.push_back(t);

		clEnqueueReadBuffer(readRpaQueue, devNextFrontierSize, CL_TRUE, 0, pad * sizeof(int), (void*)(nextFrontierSize.data), 0, NULL, NULL);
		clFinish(readRpaQueue);

		int sum = 0;
		for(int i = 0; i < pad; i++){
			sum += nextFrontierSize[i];
		}
		frontierSize = sum;

		// We just reuse the command queues here.
		for(int i = 0; i < pad/GROUP; i++){
			for(int j = 0; j < GROUP; j++){
				int idx = GROUP * i + j;
				if(nextFrontierSize[idx] > 0)
					clEnqueueReadBuffer(writeQueues[i], devNextFrontier[idx], CL_TRUE, 0, sizeof(int) * nextFrontierSize[idx], (void*)(nextFrontier[idx]), 0, NULL, NULL);
			}
		}
		for(int i = 0; i < pad/GROUP; i++){
			int sum = nextFrontierSize[GROUP * i] + nextFrontierSize[GROUP * i + 1] + nextFrontierSize[GROUP * i + 2] + nextFrontierSize[GROUP * i + 3]; 
			if(sum > 0) clFinish(writeQueues[i]);
		}
		
		int id = 0;
		for(int i = 0; i < pad; i++){
			for(int j = 0; j < nextFrontierSize[i]; j++){
				int vidx = nextFrontier[i][j];
				hwDepth[vidx] = level + 1;
				rpaInfo[2 * id] = csr->rpao[2 * vidx];
				rpaInfo[2 * id + 1] = csr->rpao[2 * vidx + 1];
				id++;
			}
		}
		
		if(frontierSize > 0){
			clEnqueueWriteBuffer(readRpaQueue, devRpa, CL_TRUE, 0, sizeof(int) * frontierSize * 2, rpaInfo.data, 0, NULL, NULL);
			clFinish(readRpaQueue);
		}
		std::cout << "hwBfs iteration: " << (int)(level) << ", frontier size: " << frontierSize << std::endl;
		level++;
	}
	end = std::chrono::high_resolution_clock::now();
	elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	double sum = 0;
	std::cout << "fpga execution time in each iteration: ";
	for(auto t : fpgaTime){
		sum += t;
		std::cout << t << " ";
	}
	std::cout << std::endl;
	std::cout << "total FPGA execution time: " << sum << std::endl;
	std::cout << "Hardware bfs time: " << elapsedTime << " ms" << std::endl;
	std::cout << "Measured MTEPS: " << 1000 * millionEdges / elapsedTime << std::endl;

	verify(swDepth, hwDepth, vertexNum);

	// Cleanup memory
	for (int i = 0; i < clContext.kernels.size(); i++){
		clReleaseKernel(kernel[i]);
	}

	clReleaseProgram(clContext.program);
	clReleaseCommandQueue(readRpaQueue);
	clReleaseCommandQueue(readCiaQueue);
	clReleaseCommandQueue(TravCiaQueue0);
	clReleaseCommandQueue(TravCiaQueue1);
	clReleaseCommandQueue(TravCiaQueue2);
	clReleaseCommandQueue(TravCiaQueue3);
	for(int i = 0; i < pad/GROUP; i++){
		clReleaseCommandQueue(writeQueues[i]);
	}
	clReleaseContext(context);
	clReleaseMemObject(devRpa);
	clReleaseMemObject(devCia);

	for(int i = 0; i < pad; i++){
		clReleaseMemObject(devNextFrontier[i]);
	}
	clReleaseMemObject(devNextFrontierSize);

	return 0;
}
