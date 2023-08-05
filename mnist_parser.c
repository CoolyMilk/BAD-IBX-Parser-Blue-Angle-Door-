#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define UBYTE  0x08
#define BYTE   0x09
#define SHORT  0x0B
#define INT    0x0C
#define FLOAT  0x0D
#define DOUBLE 0x0E

#define VALID_TYPE(X) (X == UBYTE || X == BYTE || X == SHORT || X == INT || X == FLOAT || X == DOUBLE)

struct IdxFile {
	bool corrupted;
	char file_name[128];
	unsigned int magic_number;
	unsigned int area;
	
	unsigned char data_size;
	unsigned char data_type;
	unsigned char dimensions_number;
	unsigned int *dimensions;
		
	unsigned char *data;
};

unsigned short UshortSwap(unsigned short value) {
	return (value >> 8) | (value << 8);	
}

unsigned int UintSwap(unsigned int value) {
	return ((value & 0xff) << 24) | ((value & 0xff00) << 8) | ((value & 0xff0000) >> 8) | ((value & 0xff000000) >> 24);

}

int IDXSizeType(char type) {
	
	if (type == UBYTE || type == BYTE) {
		return sizeof(char);
	
	} else if (type == SHORT) {
		return sizeof(short);
		
	} else if (type == INT) {
		return sizeof(int);
	
	} else if (type == FLOAT) {
		return sizeof(float);
	
	} else if (type == DOUBLE) {
		return sizeof(double);
	
	} else {
		return 0;
	
	}	
	
}

void printIdxFile(struct IdxFile *idx_file) {
	if (idx_file == NULL) {
		printf("[IDX_FILE COULD NOT BE PRINTED...]");
		return;
	}
	
	printf("\n[IdxFile : %p]\n", idx_file);
	printf("[File Name] : %s\n", idx_file->file_name);
	printf("[Corrupted   ] : %s\n", idx_file->corrupted ? "YES" : "NOPE");
	printf("[Magic number] : %#08x (%d)\n", idx_file->magic_number, idx_file->magic_number);
	printf("[Data Type   ] : %#02x\n", idx_file->data_type);
	printf("[No. of Dims ] : %d\n", idx_file->dimensions_number);
	        
	for (int i = 0; i < idx_file->dimensions_number; i++) {
		printf("[Dim No. %d  ] : %d\n", i, idx_file->dimensions[i]);
	}
	printf("\n");
}

void destroyIdxFile(struct IdxFile *idx_file) {
	if (idx_file == NULL) { return; }
	free(idx_file->data);
	free(idx_file->dimensions);
	free(idx_file);
}

bool safeAllocation(void **ptr, int size) { // It's probably not "safe", it's just a name because it fits for me
	int retries = 8;
	
	*ptr = (void *) calloc(1, size);
	
	for (int i = 0; i < retries; i++) {
		if ((*ptr) != NULL) { return true; }	
		*ptr = (void *) calloc(1, size);
	}
	
	return false;
}

struct IdxFile *openIdxFile(char *file_name) {
	if (file_name == NULL) { return NULL; }
	
	//Checking endian of device
	
	int n = 1;
	bool little_endian = *(char *)(&n) == 1;
	
	//Allocating the IdxFile
	
	struct IdxFile *idx_file;
	
	if (safeAllocation((void **) &idx_file, sizeof(struct IdxFile)) == false) {
		printf("Error Allocating Memory...");
		return NULL;
	}
	
	idx_file->corrupted = true;
	
	
	// Opening File
	FILE *file = fopen(file_name, "rb");
	
	if (file == NULL) {
		printf("Loading File Error...\n");
		fclose(file);
		return idx_file;
	}
	
	
	//Reading File
	
	if (fread(&(idx_file->magic_number), sizeof(int), 1, file) < 1) { 
		printf("Magic Number Failure...\n");
		fclose(file);
		return idx_file; 
	}
	
	idx_file->data_type = ((unsigned char *) &(idx_file->magic_number))[2];
	idx_file->dimensions_number = ((unsigned char *) &(idx_file->magic_number))[3];
	
	
	if (little_endian) { idx_file->magic_number = UintSwap(idx_file->magic_number); }
	
	if (safeAllocation((void **) &(idx_file->dimensions), idx_file->dimensions_number * sizeof(unsigned int)) == false) {
		printf("Error Allocating Memory...");
		fclose(file);
		return NULL;
	}
	
	
	idx_file->area = 1;
	for (int i = 0; i < idx_file->dimensions_number; i++) {
		if (fread(&(idx_file->dimensions[i]), sizeof(unsigned int), 1, file) < 1) { 
			printf("Dimensional Number Failure...\n");
			fclose(file);
			return idx_file; 
		}
		
		if (little_endian) { idx_file->dimensions[i] = UintSwap(idx_file->dimensions[i]); }
		
		idx_file->area *= idx_file->dimensions[i];
	}
	
	if (safeAllocation((void **) &(idx_file->data), idx_file->area) == false) {
		printf("Data Allocation Failure...");
		fclose(file);
		return idx_file;
	}
	
	int res;
	if ((res = fread(idx_file->data, 1, idx_file->area, file)) < idx_file->area) { 
		printf("Data Extraction Failure... %d\n", res);
		fclose(file);
		return idx_file;
	}
	
	idx_file->data_size = (unsigned int) IDXSizeType(idx_file->data_type);
	if (idx_file->data_size == 0) {
		printf("Invalid Type Failure...");
		fclose(file);
		return idx_file;
	}
	
	int counter = 0;
	while (file_name[counter] != 0 || counter < 128) {
		idx_file->file_name[counter] = file_name[counter]; counter++;
	}
	
	// Closing
	
	fclose(file);
	idx_file->corrupted = false;
	
	return idx_file;
};

int offsetIdxFile(int *index, struct IdxFile *idx_file) {
	
	if (idx_file == NULL || index == NULL) { return -1; }
	
	int final_index = 0;
	int multiplier = (int) idx_file->data_size;
	
	
	// Calculating for offset based on amount of dimensions
	
	for (int j = 0; j < idx_file->dimensions_number; j++)  {
		
		final_index += multiplier * index[j];
		multiplier *= (int) idx_file->dimensions[idx_file->dimensions_number - j - 1];
	
	}
	
	return final_index;

}

unsigned char *accessIdxFile(int *index, struct IdxFile *idx_file) {
	int offset;
	
	if (idx_file == NULL) { return NULL; }
	
	offset = offsetIdxFile(index, idx_file);
	
	if (offset == -1) { return NULL; }
	
	unsigned char *output;
	if (safeAllocation((void **) &output, idx_file->data_size) == false) {
		printf("Failed to allocate memory...");
		return NULL;
	}
	
	//Retrieving Data
	
	for (int j = 0; j < idx_file->data_size; j++) {
		output[j] = idx_file->data[offset+j];
	}
	
	return output;
}

char values[6] = {' ', '.', '-', '+', '#', '@'};
char palleteConvert(unsigned char value) {
	return values[((int) value * (sizeof(values)-1))/255];
}

int main(int argc, char **argv) {
	
	char file_names[2][64] = {"mnist\\t10k-images.idx3-ubyte", "mnist\\t10k-labels.idx1-ubyte"};
	struct IdxFile *images = openIdxFile(file_names[0]);
	struct IdxFile *labels = openIdxFile(file_names[1]);
	
	printIdxFile(images);
	printIdxFile(labels);
	printf("\n\n\n");
	
	int index[3];
	int label_index[1];
	
	char buffer[28*29+1];
	int buffer_index = 0;
	
	for (int k = 0; k < images->dimensions[0]; k++) {
		buffer_index = 0;
		
		index[2] = k;
		label_index[0] = k;
		
		for (int y = 0; y < images->dimensions[1]; y++) {
			for (int x = 0; x < images->dimensions[2]; x++) {
				
				index[0] = x;
				index[1] = y;
				
				buffer[buffer_index++] = palleteConvert(*accessIdxFile(index, images));
			}
			buffer[buffer_index++] = '\n';
		}
		
		printf("%s\n", buffer);
		printf("[Current Label] : 0b%d\n", *((unsigned short * ) accessIdxFile(label_index, labels)));
		
		char output;
		scanf("%c", &output);
	}
	
	return 0;
}
