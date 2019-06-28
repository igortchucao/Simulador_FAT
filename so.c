/*INCLUDE*/
#include "so.h"

/*DATA DECLARATION*/
unsigned short fat[NUM_CLUSTER];
unsigned char boot_block[CLUSTER_SIZE];
dir_entry_t root_dir[32];
data_cluster clusters[4086];

void init(void)
{
	FILE* ptr_file;
	int i;
	ptr_file = fopen(fat_name,"wb");
	for (i = 0; i < CLUSTER_SIZE; ++i)
		boot_block[i] = 0xbb;

	fwrite(&boot_block, sizeof(boot_block), 1,ptr_file);

	fat[0] = 0xfffd;
	for (i = 1; i < 9; ++i)
		fat[i] = 0xfffe;

	fat[9] = 0xffff;
	for (i = 10; i < NUM_CLUSTER; ++i)
		fat[i] = 0x0000;

	fwrite(&fat, sizeof(fat), 1, ptr_file);
	fwrite(&root_dir, sizeof(root_dir), 1,ptr_file);

	for (i = 0; i < 4086; ++i)
		fwrite(&clusters, sizeof(data_cluster), 1, ptr_file);

	fclose(ptr_file);
}

void carregaFat(void)
{
	int i;
	FILE* fatFile;
	fatFile = fopen(fat_name, "rb");
	fseek(fatFile, sizeof(boot_block), 0);
	fread(fat, sizeof(fat), 1, fatFile);
	fclose(fatFile);
}

void salvaFat(void)
{
	int i;
	FILE* fatFile;
	fatFile = fopen(fat_name, "r+b");
	fseek(fatFile, sizeof(boot_block), 0);
	fwrite(fat, sizeof(fat), 1, fatFile);
	fclose(fatFile);
}

//
data_cluster* carregaFatCluster(int block)
{	
	//Cria um cluster para poder retornar 
	data_cluster* cluster;
	cluster = calloc(1, sizeof(data_cluster));
	//Abre o arquivo da fat
	FILE* fatFile;
	fatFile = fopen(fat_name, "rb");
	//Acessa no arquivo da fat a posição em que o bloco se encontra
	fseek(fatFile, block*sizeof(data_cluster), 0);
	//Lê o cluster que esta na fat e poem ele na variavel cluster
	fread(cluster, sizeof(data_cluster), 1, fatFile);
	fclose(fatFile);
	//Retorna o cluster lido
	return cluster;
}

data_cluster* escreveCluster(int block, data_cluster* cluster)
{
	FILE* fatFile;
	fatFile = fopen(fat_name, "r+b");
	fseek(fatFile, block*sizeof(data_cluster), 0);
	fwrite(cluster, sizeof(data_cluster), 1, fatFile);
	fclose(fatFile);
}

void apagaCluster(int block)
{
	data_cluster* cluster;
	cluster = calloc(1, sizeof(data_cluster));
	escreveCluster(block, cluster);
}

data_cluster* encontraPrincipal(data_cluster* current_cluster, char* path, int* addr)
{
	char path_aux[strlen(path)];
	strcpy(path_aux, path);
	char* dir_name = strtok(path_aux, "/");
	char* rest     = strtok(NULL, "\0");

	dir_entry_t* current_dir = current_cluster->dir;

	int i=0;
	while (i < 32) {
		dir_entry_t child = current_dir[i];
		if (strcmp(child.filename, dir_name) == 0 && rest){
			data_cluster* cluster = carregaFatCluster(child.first_block);
			*addr = child.first_block;
			return encontraPrincipal(cluster, rest, addr);
		}
		else if (strcmp(child.filename, dir_name) == 0 && rest){
			return NULL;
		}
		i++;
	}

	if (!rest)
		return current_cluster;

	return NULL;
}

data_cluster* encontra(data_cluster* current_cluster, char* path, int* addr)
{
	if (!path || strcmp(path, "/") == 0)
		return current_cluster;

	char path_aux[strlen(path)];
	strcpy(path_aux, path);
	char* dir_name = strtok(path_aux, "/");
	char* rest     = strtok(NULL, "\0");

	dir_entry_t* current_dir = current_cluster->dir;

	int i=0;
	while (i < 32) {
		dir_entry_t child = current_dir[i];
		if (strcmp(child.filename, dir_name) == 0){
			data_cluster* cluster = carregaFatCluster(child.first_block);
			*addr = child.first_block;
			return encontra(cluster, rest, addr);
		}
		i++;
	}
	return NULL;
}

char* get_name(char* path)
{

	char name_aux[strlen(path)];
	strcpy(name_aux, path);

	char* name = strtok(name_aux, "/");
	char* rest = strtok(NULL, "\0");
	if (rest != NULL)
		return get_name(rest);

	return (char*) name;
}

int encontra_free_space(dir_entry_t* dir)
{
	int i;
	for (i = 0; i < 32; ++i){
		if (!dir->attributes)
			return i;
		dir++;
	}
	return -1;
}

void copy_str(char* copia, char* copiado)
{
	int len = strlen(copiado);
	int i;
	for (i = 0; i < len; ++i) {
		copia[i] = copiado[i];
	}
}

void limpaString(char* s)
{
	s = (unsigned char*)calloc(4096,sizeof(unsigned char));
}

int search_fat_free_block(void)
{
	carregaFat();
	int i;
	for (i = 10; i < 4096; ++i)
		if(!fat[i]){
			fat[i] = -1;
			salvaFat();
			return i;
		}
	return 0;
}

void mkdir(char* path)
{
	if(path == "/")
		return;

	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster_parent = encontraPrincipal(root_cluster, path, &root_addr);

	if (cluster_parent){
		int free_position = encontra_free_space(cluster_parent->dir);
		int fat_block = search_fat_free_block();
		if (fat_block && free_position != -1) {
			char* dir_name = get_name(path);
			copy_str(cluster_parent->dir[free_position].filename, dir_name);
			cluster_parent->dir[free_position].attributes = 1;
			cluster_parent->dir[free_position].first_block = fat_block;
			escreveCluster(root_addr, cluster_parent);
		}
	}
	else
		printf("PATH NOT FOUND\n");
}

void ls(char* path)
{
	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, path, &root_addr);
	int i;
	if (cluster){
		for (i = 0; i < 32; ++i){
			if (cluster->dir[i].attributes == 1 || cluster->dir[i].attributes == 2)
				printf("%s\n", cluster->dir[i].filename);
		}
	}
	else
		printf("PATH NOT FOUND\n");
}

int temFilho(char* path)
{
	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, path, &root_addr);
	int i;
	if (cluster){
		for (i = 0; i < 32; ++i){
			if (cluster->dir[i].attributes == 1 || cluster->dir[i].attributes == 2)
				return 1;
		}
	}
	else
		return 0;
}

void create(char* path)
{
	if(path == "/")
		return;

	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster_parent = encontraPrincipal(root_cluster, path, &root_addr);

	if (cluster_parent){
		int free_position = encontra_free_space(cluster_parent->dir);
		int fat_block = search_fat_free_block();
		if (fat_block && free_position != -1) {
			char* dir_name = get_name(path);
			copy_str(cluster_parent->dir[free_position].filename, dir_name);
			cluster_parent->dir[free_position].attributes = 2;
			cluster_parent->dir[free_position].first_block = fat_block;
			escreveCluster(root_addr, cluster_parent);
		}
	}
	else
		printf("PATH NOT FOUND\n");
}

void write(char* path, char* content)
{
	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, path, &root_addr);
	if (cluster){
		limpaString(cluster->data);
		copy_str(cluster->data, content);
		escreveCluster(root_addr, cluster);
	}
	else
		printf("FILE NOT FOUND\n");

}

int empty(int block)
{
	data_cluster* cluster = carregaFatCluster(block);
	int i;
	for (i = 0; i < 32; ++i)
		if(cluster->dir[i].attributes != 0)
			return 0;

	return 1;
}

void unlink(char* path)
{
	carregaFat();
	int i;
	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(root_addr);
	encontraPrincipal(root_cluster, path, &root_addr);
	data_cluster* cluster = carregaFatCluster(root_addr);
	if(temFilho(path) == 1){
	printf("Não é possível excluir pois não está vazio");
	}else{
		if (cluster != NULL) {
			char* name = get_name(path);
			for (i = 0; i < 32; ++i) {
				if (strcmp(cluster->dir[i].filename, name)==0) {
					cluster->dir[i].attributes = -1;
					
					int first = cluster->dir[i].first_block;
					fat[first] = 0x0000;
					salvaFat();
					apagaCluster(root_addr);
				
				}
			}
		}else{
			printf("FILE NOT FOUND UNLINK\n");
		}
	}

}

void read(char* path)
{
	carregaFat();
	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, path, &root_addr);
	if (cluster)
		printf("%s\n", cluster->data);

	else
		printf("FILE NOT FOUND\n");
}

void append(char* path, char* content)
{
	carregaFat();
	int root_addr = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, path, &root_addr);
	if (cluster){
		char* data = cluster->data;
		strcat(data, content);
		copy_str(cluster->data, data);
		escreveCluster(root_addr, cluster);
	}

	else
		printf("FILE NOT FOUND\n");

}

void shell(void)
{
	char name[4096] = { 0 };
    char name2[4096] = { 0 };
	char nameCopy[4096] = { 0 };
	const char aux[2] = "/";
	char aux2[4096] = { 0 };

	char *token;
	int i;

	fgets(name,4096,stdin);

	strcpy(nameCopy,name);

	token = strtok(name,aux);

	if ( strcmp(token, "append ") == 0 && nameCopy[7] == '/')
	{
		for(i = 7; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-7] = nameCopy[i];
		}
		printf("Digite o texto");
		fgets(name2,4096,stdin);
		append(aux2,name2);
	}
	else if ( strcmp(token, "create ") == 0 && nameCopy[7] == '/')
	{
		for(i = 7; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-7] = nameCopy[i];
		}
		create(aux2);
	}
	else if ( strcmp(token, "init\n") == 0)
	{
		init();
	}
	else if ( strcmp(token, "load\n") == 0)
	{
		carregaFat();
	}
	else if ( strcmp(token, "ls ") == 0 && nameCopy[3] == '/') 
	{
		for(i = 3; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-3] = nameCopy[i];
		}
		ls(aux2);
	}
	else if ( strcmp(token, "mkdir ") == 0 && nameCopy[6] == '/')
	{
		for(i = 6; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-6] = nameCopy[i];
		}
		mkdir(aux2);
	}
	else if ( strcmp(token, "read ") == 0 && nameCopy[5] == '/')
	{
		for(i = 5; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-5] = nameCopy[i];
		}
		read(aux2);
	}
	else if ( strcmp(token, "unlink ") == 0 && nameCopy[7] == '/')
	{
		for(i = 7; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-7] = nameCopy[i];
		}
		unlink(aux2);
	}
	else if ( strcmp(token, "write ") == 0 && nameCopy[6] == '/')
	{
		for(i = 6; i < strlen(nameCopy)-1; ++i)
		{
			aux2[i-6] = nameCopy[i];
		}
		printf("Digite o texto:\n");
		fgets(name2,4096,stdin);
		write(aux2,name2);
	}
	else printf("Erro no comando \n");
}
