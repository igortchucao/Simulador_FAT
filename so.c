/*INCLUDE*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*DEFINE*/
#define SECTOR_SIZE	512
#define CLUSTER_SIZE	2*SECTOR_SIZE
#define ENTRY_BY_CLUSTER CLUSTER_SIZE /sizeof(dir_entry_t)
#define NUM_CLUSTER	4096
#define fat_name	"fat.part"

struct _dir_entry_t
{
	unsigned char filename[18];
	unsigned char attributes;
	unsigned char reserved[7];
	unsigned short first_block;
	unsigned int size;
};

typedef struct _dir_entry_t  dir_entry_t; 

union _data_cluster
{
	dir_entry_t dir[CLUSTER_SIZE / sizeof(dir_entry_t)];
	unsigned char data[CLUSTER_SIZE];
};

typedef union _data_cluster data_cluster;

/*DATA DECLARATION*/
unsigned short fat[NUM_CLUSTER];
unsigned char boot_block[CLUSTER_SIZE];
dir_entry_t root_dir[32];
data_cluster clusters[4086];

/*Function declaration*/
void append(char* caminho, char* content);
void create(char* caminho);
void mkdir (char* caminho);
void read  (char* caminho);
void unlink(char* caminho);
int  encontraEspacoVazio(dir_entry_t* dir);

int init(void)
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
	return 1;
}

//Carrega uma fat já existente
int carregaFat(void)
{
	int i;
	FILE* fatFile;
	fatFile = fopen(fat_name, "rb");
	//Caso não exista o arquivo fat.part
	if(fatFile == NULL){
		printf("Fat não achada, use o comando init para inicia-la \n");
		return 0;
	}else{
	//Caso exista carrega a fat
	fseek(fatFile, sizeof(boot_block), 0);
	fread(fat, sizeof(fat), 1, fatFile);
	fclose(fatFile);
		return 1;
	}
}

//Salva a fat no arquivo fat.part
void salvaFat(void)
{
	int i;
	FILE* fatFile;
	fatFile = fopen(fat_name, "r+b");
	fseek(fatFile, sizeof(boot_block), 0);
	fwrite(fat, sizeof(fat), 1, fatFile);
	fclose(fatFile);
}

//Carrega um cluster da fat
data_cluster* carregaFatCluster(int bloco)
{	
	//Cria um cluster para poder retornar 
	data_cluster* cluster;
	cluster = calloc(1, sizeof(data_cluster));
	//Abre o arquivo da fat
	FILE* fatFile;
	fatFile = fopen(fat_name, "rb");
	//Acessa no arquivo da fat a posição em que o bloco se encontra
	fseek(fatFile, bloco*sizeof(data_cluster), 0);
	//Lê o cluster que esta na fat e poem ele na variavel cluster
	fread(cluster, sizeof(data_cluster), 1, fatFile);
	fclose(fatFile);
	//Retorna o cluster lido
	return cluster;
}

//Escreve em um clsuter da fat
data_cluster* escreveCluster(int bloco, data_cluster* cluster)
{
	FILE* fatFile;
	fatFile = fopen(fat_name, "r+b");
	//Acessa o arquivo da fat na posição passada do bloco
	fseek(fatFile, bloco*sizeof(data_cluster), 0);
	//Escreve no cluster que está na fat
	fwrite(cluster, sizeof(data_cluster), 1, fatFile);
	fclose(fatFile);
}

//Utiliza-se do calloc para zerar um cluster
void apagaCluster(int bloco)
{
	data_cluster* cluster;
	cluster = calloc(1, sizeof(data_cluster));
	escreveCluster(bloco, cluster);
}

//Encontra o diretório em que está atualmente
data_cluster* encontraPrincipal(data_cluster* clusterAtual, char* caminho, int* endereco)
{

	char caminhoAux[strlen(caminho)];
	strcpy(caminhoAux, caminho);
	char* nomeDiretorio = strtok(caminhoAux, "/");
	char* resto = strtok(NULL, "\0");

	dir_entry_t* current_dir = clusterAtual->dir;

	int i=0;
	while (i < 32) {
		dir_entry_t child = current_dir[i];
		if (strcmp(child.filename, nomeDiretorio) == 0 && resto){
			data_cluster* cluster = carregaFatCluster(child.first_block);
			*endereco = child.first_block;
			return encontraPrincipal(cluster, resto, endereco);
		}
		else if (strcmp(child.filename, nomeDiretorio) == 0 && resto){
			return NULL;
		}
		i++;
	}

	if (!resto)
		return clusterAtual;

	return NULL;
}
//Encontra os diretórios que estão no caminho passado 
data_cluster* encontra(data_cluster* clusterAtual, char* caminho, int* endereco)
{
	if (!caminho || strcmp(caminho, "/") == 0)
		return clusterAtual;

	char caminhoAux[strlen(caminho)];
	strcpy(caminhoAux, caminho);
	char* nomeDiretorio = strtok(caminhoAux, "/");
	char* resto = strtok(NULL, "\0");

	dir_entry_t* current_dir = clusterAtual->dir;

	int i=0;
	while (i < 32) {
		dir_entry_t child = current_dir[i];
		if (strcmp(child.filename, nomeDiretorio) == 0){
			data_cluster* cluster = carregaFatCluster(child.first_block);
			*endereco = child.first_block;
			return encontra(cluster, resto, endereco);
		}
		i++;
	}
	return NULL;
}

//Retorna o nome do caminho passado
char* pegaNome(char* caminho)
{

	char nomeAuxiliar[strlen(caminho)];
	strcpy(nomeAuxiliar, caminho);

	char* nome = strtok(nomeAuxiliar, "/");
	char* resto = strtok(NULL, "\0");
	if (resto != NULL)
		return pegaNome(resto);

	return (char*) nome;
}

//Encontra um espaço que não tenha nada no diretorio
int encontraEspacoVazio(dir_entry_t* dir)
{
	int i;
	for (i = 0; i < 32; ++i){
		if (!dir->attributes)
			return i;
		dir++;
	}
	return -1;
}

//Faz uma copia de string
void copiaString(char* copia, char* copiado)
{
	int tam = strlen(copiado);
	int i;
	for (i = 0; i < tam; ++i) {
		copia[i] = copiado[i];
	}
}

//Usa calloc para limpar a string (utilizado no write para sobrescrever)
void limpaString(char* s)
{
	s = (unsigned char*)calloc(CLUSTER_SIZE,sizeof(unsigned char));
}

//Procura um bloco na fat que retone -1 (Livre)
int procuraBlocoLivre(void)
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

void mkdir(char* caminho)
{
	if(caminho == "/")
		return;

	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster_parent = encontraPrincipal(root_cluster, caminho, &root_endereco);

	if (cluster_parent){
		int free_position = encontraEspacoVazio(cluster_parent->dir);
		int fat_block = procuraBlocoLivre();
		if (fat_block && free_position != -1) {
			char* nomeDiretorio = pegaNome(caminho);
			copiaString(cluster_parent->dir[free_position].filename, nomeDiretorio);
			cluster_parent->dir[free_position].attributes = 1;
			cluster_parent->dir[free_position].first_block = fat_block;
			escreveCluster(root_endereco, cluster_parent);
		}
	}
	else
		printf("caminho NOT FOUND\n");
}

void ls(char* caminho)
{
	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, caminho, &root_endereco);
	int i;
	if (cluster){
		for (i = 0; i < 32; ++i){
			if (cluster->dir[i].attributes == 1 || cluster->dir[i].attributes == 2)
				printf("%s\n", cluster->dir[i].filename);
		}
	}
	else
		printf("caminho NOT FOUND\n");
}

int temFilho(char* caminho)
{
	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, caminho, &root_endereco);
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

void create(char* caminho)
{
	if(caminho == "/")
		return;

	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster_parent = encontraPrincipal(root_cluster, caminho, &root_endereco);

	if (cluster_parent){
		int free_position = encontraEspacoVazio(cluster_parent->dir);
		int fat_block = procuraBlocoLivre();
		if (fat_block && free_position != -1) {
			char* nomeDiretorio = pegaNome(caminho);
			copiaString(cluster_parent->dir[free_position].filename, nomeDiretorio);
			cluster_parent->dir[free_position].attributes = 2;
			cluster_parent->dir[free_position].first_block = fat_block;
			escreveCluster(root_endereco, cluster_parent);
		}
	}
	else
		printf("caminho NOT FOUND\n");
}

void write(char* caminho, char* content)
{
	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, caminho, &root_endereco);
	if (cluster){
		limpaString(cluster->data);
		copiaString(cluster->data, content);
		escreveCluster(root_endereco, cluster);
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

void unlink(char* caminho)
{
	carregaFat();
	int i;
	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(root_endereco);
	encontraPrincipal(root_cluster, caminho, &root_endereco);
	data_cluster* cluster = carregaFatCluster(root_endereco);
	if(temFilho(caminho) == 1){
	printf("Não é possível excluir pois não está vazio");
	}else{
		if (cluster != NULL) {
			char* name = pegaNome(caminho);
			for (i = 0; i < 32; ++i) {
				if (strcmp(cluster->dir[i].filename, name)==0) {
					cluster->dir[i].attributes = -1;
					
					int first = cluster->dir[i].first_block;
					fat[first] = 0x0000;
					salvaFat();
					apagaCluster(root_endereco);
				
				}
			}
		}else{
			printf("FILE NOT FOUND UNLINK\n");
		}
	}

}

void read(char* caminho)
{
	carregaFat();
	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, caminho, &root_endereco);
	if (cluster)
		printf("%s\n", cluster->data);

	else
		printf("FILE NOT FOUND\n");
}

void append(char* caminho, char* content)
{
	carregaFat();
	int root_endereco = 9;
	data_cluster* root_cluster = carregaFatCluster(9);
	data_cluster* cluster = encontra(root_cluster, caminho, &root_endereco);
	if (cluster){
		char* data = cluster->data;
		strcat(data, content);
		copiaString(cluster->data, data);
		escreveCluster(root_endereco, cluster);
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

	printf("Digite o comando: ");
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

void comandoInicializar(){
	int aux = 0;
	while(aux != 1){
		char nome[4096] = { 0 };
		printf("Digite o comando para começar (init ou load):  ");
		fgets(nome,4096,stdin);
		if ( strcmp(nome, "init\n") == 0)
		{
			aux = init();
		}
		else if ( strcmp(nome, "load\n") == 0)
		{
			aux = carregaFat();
		}
	}
}
