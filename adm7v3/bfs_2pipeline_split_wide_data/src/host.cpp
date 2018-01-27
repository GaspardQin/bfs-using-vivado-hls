/**********
Author: Cheng Liu
Email: st.liucheng@gmail.com
Software: SDx 2016.4
Date: July 6th 2017
**********/

#include "xcl.h"
#include "graph.h"

#include <cstdio>
#include <vector>
#include <ctime>

static const char *error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

#define OCL_CHECK(call)                                                        \
	do {                                                                       \
		cl_int err = call;                                                     \
		if (err != CL_SUCCESS) {                                               \
			printf("Error calling " #call ", error: %s\n", oclErrorCode(err)); \
			exit(EXIT_FAILURE);                                                \
		}                                                                      \
	} while (0);

Graph* createGraph(const std::string &gName){
	Graph* gptr;
	if(gName == "dblp"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/dblp.ungraph.txt");
	}
	else if(gName == "youtube"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/youtube.ungraph.txt");
	}
	else if(gName == "lj"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/lj.ungraph.txt");
	}
	else if(gName == "pokec"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/pokec-relationships.txt");
	}
	else if(gName == "wiki-talk"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/wiki-Talk.txt");
	}
	else if(gName == "lj1"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/LiveJournal1.txt");
	}
	else if(gName == "orkut"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/orkut.ungraph.txt");
	}
	else if(gName == "rmat"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-2m-256m.txt");
	}
	else if(gName == "rmat-19-32"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-19-32.txt");
	}
	else if(gName == "rmat21-128"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-21-128.txt");
	}
	else if(gName == "twitter"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/twitter_rv.txt");
	}
	else if(gName == "friendster"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/friendster.ungraph.txt");
	}
	else if(gName == "example"){
		gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-1k-10k.txt");
	}
	else{
		std::cout << "Unknown graph name." << std::endl;
		exit(EXIT_FAILURE);
	}

	return gptr;
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
		//std::cout << "swBfs: level = " << (int)level << std::endl;

		// frontier inspection
		for(int i = 0; i < csr->vertexNum; i++){
			if(depth[i] == level){
				frontier.push_back(i);
				for(int cidx = csr->rpao[i]; cidx < csr->rpao[i+1]; cidx++){
					int ongb = csr->ciao[cidx];
					if(depth[ongb] == -1){
						depth[ongb] = level + 1;
					}
				}
			}
		}

		if(frontier.empty()){
			//std::cout << "software bfs is done." << std::endl;
			break;
		}

		level++;
		//std::cout << "frontier size = " << frontier.size() << std::endl;
		frontier.clear();
	}
}

void hwBfsInit(int vertexNum, char* depth, int startVertexIdx){
	for(int i = 0; i < vertexNum; i++){
		if(i == startVertexIdx){
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
			printf(error_message, i, swDepth[i], hwDepth[i]);	
			match = false;
			break;
		} 
	}

	if (match) {
		printf("TEST PASSED.\n");
		return EXIT_SUCCESS;
	} else {
		printf("TEST FAILED.\n");
		return EXIT_FAILURE;
	}
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


int getTotalHubVertexNum(CSR* csr, int degreeThreshold){
	int outHubVertexNum = 0;
	int inHubVertexNum = 0;
	for(size_t i = 1; i < csr->rpao.size(); i++){
		if(csr->rpao[i] - csr->rpao[i - 1] > degreeThreshold){
			outHubVertexNum++;
		}
	}

	for(size_t i = 1; i < csr->rpai.size(); i++){
		if(csr->rpai[i] - csr->rpai[i - 1] > degreeThreshold){
			inHubVertexNum++;
		}
	}

	std::cout << "HubVertexNum(out): " << outHubVertexNum << std::endl;
	std::cout << "HubVertexNum(in): " << inHubVertexNum << std::endl;

	return outHubVertexNum;
}

int main(int argc, char **argv) {
	std::clock_t begin;
	std::clock_t end;
	double elapsedTime;

	int startVertexIdx = 365723;
	std::string gName = "youtube";
	if(gName == "youtube") startVertexIdx = 320872;
	if(gName == "lj1") startVertexIdx = 3928512;
	if(gName == "pokec") startVertexIdx = 182045;
	if(gName == "rmat-19-32") startVertexIdx = 104802;
	if(gName == "rmat-21-128") startVertexIdx = 1177730;
	Graph* gptr = createGraph(gName);
	CSR* csr = new CSR(*gptr);
	free(gptr);
	std::cout << "Graph is loaded." << std::endl;
	int vertexNum = align(csr->vertexNum, 8, 1024); 
	std::cout << "verterNum " << csr->vertexNum << " is aligned to " << vertexNum << std::endl;

	char *hwDepth = (char*)malloc(vertexNum * sizeof(char));
	char *swDepth = (char*)malloc(vertexNum * sizeof(char));
	int* frontierSize = (int*)malloc(2*sizeof(int));	

	int ciaoSize = csr->ciao.size();
	int rpaoSize = csr->rpao.size();

	std::cout << "soft bfs starts." << std::endl;
	swBfsInit(vertexNum, swDepth, startVertexIdx);
	begin = clock();
	swBfs(csr, swDepth, startVertexIdx);
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	std::cout << "Software bfs takes " << elapsedTime << " seconds." << std::endl;

	hwBfsInit(vertexNum, hwDepth, startVertexIdx);
	xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "bfs");
	cl_kernel krnl_bfs = xcl_get_kernel(program, "bfs");

    // Transfer data from system memory over PCIe to the FPGA on-board DDR memory.
    cl_mem devMemRpao = xcl_malloc(world, CL_MEM_READ_ONLY, rpaoSize * sizeof(int));
	cl_mem devMemCiao = xcl_malloc(world, CL_MEM_READ_ONLY, ciaoSize * sizeof(int));
    cl_mem devMemDepth = xcl_malloc(world, CL_MEM_READ_WRITE, vertexNum * sizeof(char));
    cl_mem devMemFrontierSize = xcl_malloc(world, CL_MEM_WRITE_ONLY, 2*sizeof(int));

    xcl_memcpy_to_device(world, devMemRpao, csr->rpao.data(), rpaoSize * sizeof(int));
    xcl_memcpy_to_device(world, devMemCiao, csr->ciao.data(), ciaoSize * sizeof(int));
    xcl_memcpy_to_device(world, devMemDepth, hwDepth, vertexNum* sizeof(char));

	int nargs = 0;
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemRpao);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemRpao);

	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemCiao);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemCiao);

	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemDepth);

	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(cl_mem), &devMemFrontierSize);
	xcl_set_kernel_arg(krnl_bfs, nargs++, sizeof(int), &vertexNum);

	char level = 0;
	begin = clock();
	int totalFrontierSize = 1;
	while(totalFrontierSize > 0){
		xcl_set_kernel_arg(krnl_bfs, nargs, sizeof(char), &level);
		xcl_run_kernel3d(world, krnl_bfs, 1, 1, 1);
		xcl_memcpy_from_device(world, frontierSize, devMemFrontierSize, 2*sizeof(int));
		totalFrontierSize = frontierSize[0] + frontierSize[1];
		level++;
	}
	clFinish(world.command_queue);
	end = clock();
	elapsedTime = (end - begin)*1.0/CLOCKS_PER_SEC;
	xcl_memcpy_from_device(world, hwDepth, devMemDepth, vertexNum * sizeof(char));
	std::cout << "hardware bfs takes " << elapsedTime << " seconds." << std::endl;
	std::cout << "level = " << (int)level << std::endl;
	
	clReleaseMemObject(devMemRpao);
	clReleaseMemObject(devMemCiao);
	clReleaseMemObject(devMemDepth);
	clReleaseMemObject(devMemFrontierSize);
	clReleaseKernel(krnl_bfs);
	clReleaseProgram(program);
	xcl_release_world(world);

	verify(swDepth, hwDepth, csr->vertexNum);
	free(swDepth);
	free(hwDepth);
	free(csr);

	return 0;
}