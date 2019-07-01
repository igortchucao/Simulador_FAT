/*INCLUDE*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*DEFINE*/
#define TAMANHO_SETOR	512
#define TAMANHO_CLUSTER	2*TAMANHO_SETOR
#define ENTRAR_PELO_CLUSTER TAMANHO_CLUSTER /sizeof(dir_entry_t)
#define NUMERO_CLUSTER	4096
#define NOME_FAT	"fat.part"

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
	dir_entry_t dir[TAMANHO_CLUSTER / sizeof(dir_entry_t)];
	unsigned char data[TAMANHO_CLUSTER];
};

typedef union _data_cluster data_cluster;

/*DECLARAÇÃO DE DADOS*/
unsigned short fat[NUMERO_CLUSTER];
unsigned char boot_block[TAMANHO_CLUSTER];
dir_entry_t root_dir[32];
data_cluster clusters[4086];

/*DECLARAÇÃO DE FUNÇÕES DO SHELL*/
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
	ptr_file = fopen(NOME_FAT,"wb");
	for (i = 0; i < TAMANHO_CLUSTER; ++i)
		boot_block[i] = 0xbb;

	fwrite(&boot_block, sizeof(boot_block), 1,ptr_file);

	fat[0] = 0xfffd;
	for (i = 1; i < 9; ++i)
		fat[i] = 0xfffe;

	fat[9] = 0xffff;
	for (i = 10; i < NUMERO_CLUSTER; ++i)
		fat[i] = 0x0000;

	fwrite(&fat, sizeof(fat), 1, ptr_file);
	fwrite(&root_dir, sizeof(root_dir), 1,ptr_file);

	for (i = 0; i < 4086; ++i)
		fwrite(&clusters, sizeof(data_cluster), 1, ptr_file);

	fclose(ptr_file);
	return 1;
}

//Salva a fat no arquivo fat.part
void salvaFat(void)
{
	int i;
	FILE* arqFAT;
	arqFAT = fopen(NOME_FAT, "r+b");
	fseek(arqFAT, sizeof(boot_block), 0);
	fwrite(fat, sizeof(fat), 1, arqFAT);
	fclose(arqFAT);
}

//Carrega uma fat já existente
int carregaFat(void)
{
	int i;
	FILE* arqFAT;
	arqFAT = fopen(NOME_FAT, "rb");
	//Caso não exista o arquivo fat.part
	if(arqFAT == NULL){
		printf("Fat não achada, use o comando init para inicia-la \n");
		return 0;
	}else{
	//Caso exista carrega a fat
	fseek(arqFAT, sizeof(boot_block), 0);
	fread(fat, sizeof(fat), 1, arqFAT);
	fclose(arqFAT);
		return 1;
	}
}

//Carrega um cluster da fat
data_cluster* carregaFatCluster(int bloco)
{	
	//Cria um cluster para poder retornar 
	data_cluster* cluster;
	cluster = calloc(1, sizeof(data_cluster));
	//Abre o arquivo da fat
	FILE* arqFAT;
	arqFAT = fopen(NOME_FAT, "rb");
	//Acessa no arquivo da fat a posição em que o bloco se encontra
	fseek(arqFAT, bloco*sizeof(data_cluster), 0);
	//Lê o cluster que esta na fat e poem ele na variavel cluster
	fread(cluster, sizeof(data_cluster), 1, arqFAT);
	fclose(arqFAT);
	//Retorna o cluster lido
	return cluster;
}

//Escreve em um clsuter da fat
data_cluster* escreveCluster(int bloco, data_cluster* cluster)
{
	FILE* arqFAT;
	arqFAT = fopen(NOME_FAT, "r+b");
	//Acessa o arquivo da fat na posição passada do bloco
	fseek(arqFAT, bloco*sizeof(data_cluster), 0);
	//Escreve no cluster que está na fat
	fwrite(cluster, sizeof(data_cluster), 1, arqFAT);
	fclose(arqFAT);
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
		dir_entry_t filho = current_dir[i];
		if (strcmp(filho.filename, nomeDiretorio) == 0 && resto){
			data_cluster* cluster = carregaFatCluster(filho.first_block);
			*endereco = filho.first_block;
			return encontraPrincipal(cluster, resto, endereco);
		}
		else if (strcmp(filho.filename, nomeDiretorio) == 0 && resto){
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
		dir_entry_t filho = current_dir[i];
		if (strcmp(filho.filename, nomeDiretorio) == 0){
			data_cluster* cluster = carregaFatCluster(filho.first_block);
			*endereco = filho.first_block;
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

//Usa calloc para limpar a string (utilizado no write para sobrescrever)
void limpaString(char* s)
{
	s = (unsigned char*)calloc(TAMANHO_CLUSTER,sizeof(unsigned char));
}
void ls(char* caminho)
{
	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* cluster = encontra(clusterDoRoot, caminho, &enderecoRoot);
	int i;
	if (cluster){
		for (i = 0; i < 32; ++i){
			if (cluster->dir[i].attributes == 1 || cluster->dir[i].attributes == 2)
				printf("%s\n", cluster->dir[i].filename);
		}
	}
	else
		printf("caminho não encontrado\n");
}

//Função para criar um diretorio
void mkdir(char* caminho)
{
	if(caminho == "/")
		return;

	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* clusterPrincipal = encontraPrincipal(clusterDoRoot, caminho, &enderecoRoot);

	if (clusterPrincipal){
		int posicaoLivre = encontraEspacoVazio(clusterPrincipal->dir);
		int blocoFat = procuraBlocoLivre();
		if (blocoFat && posicaoLivre != -1) {
			char* nomeDiretorio = pegaNome(caminho);
			copiaString(clusterPrincipal->dir[posicaoLivre].filename, nomeDiretorio);
			clusterPrincipal->dir[posicaoLivre].attributes = 1;
			clusterPrincipal->dir[posicaoLivre].first_block = blocoFat;
			escreveCluster(enderecoRoot, clusterPrincipal);
		}
	}
	else
		printf("caminho não encontrado\n");
}


//Função que retorna 1 caso tenha algum diretorio ou arquivo dentro do diretorio olhado
int temFilho(char* caminho)
{
	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* cluster = encontra(clusterDoRoot, caminho, &enderecoRoot);
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

//Função que cria um arquivo
void create(char* caminho)
{
	if(caminho == "/")
		return;

	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* clusterPrincipal = encontraPrincipal(clusterDoRoot, caminho, &enderecoRoot);

	if (clusterPrincipal){
		int posicaoLivre = encontraEspacoVazio(clusterPrincipal->dir);
		int blocoFat = procuraBlocoLivre();
		if (blocoFat && posicaoLivre != -1) {
			char* nomeDiretorio = pegaNome(caminho);
			copiaString(clusterPrincipal->dir[posicaoLivre].filename, nomeDiretorio);
			clusterPrincipal->dir[posicaoLivre].attributes = 2;
			clusterPrincipal->dir[posicaoLivre].first_block = blocoFat;
			escreveCluster(enderecoRoot, clusterPrincipal);
		}
	}
	else
			printf("caminho não encontrado\n");
}

//Função que remove um arquivo ou diretorio
void unlink(char* caminho)
{
	carregaFat();
	int i;
	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(enderecoRoot);
	encontraPrincipal(clusterDoRoot, caminho, &enderecoRoot);
	data_cluster* cluster = carregaFatCluster(enderecoRoot);
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
					apagaCluster(enderecoRoot);
				
				}
			}
		}else{
			printf("Arquivo não encontrado\n");
		}
	}

}

//Funçao para sobrescrever em um arquivo existente
void write(char* caminho, char* content)
{	
	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* cluster = encontra(clusterDoRoot, caminho, &enderecoRoot);
	if (cluster){
		limpaString(cluster->data);
		copiaString(cluster->data, content);
		escreveCluster(enderecoRoot, cluster);
	}
	else
		printf("caminho não encontrado\n");

}

//Adiciona um texto a um arquivo existente
void append(char* caminho, char* content)
{
	carregaFat();
	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* cluster = encontra(clusterDoRoot, caminho, &enderecoRoot);
	if (cluster){
		char* data = cluster->data;
		strcat(data, content);
		copiaString(cluster->data, data);
		escreveCluster(enderecoRoot, cluster);
	}

	else
		printf("Arquivo não encontrado\n");

}

//Função para ler dados de um arquivo existente
void read(char* caminho)
{
	carregaFat();
	int enderecoRoot = 9;
	data_cluster* clusterDoRoot = carregaFatCluster(9);
	data_cluster* cluster = encontra(clusterDoRoot, caminho, &enderecoRoot);
	if (cluster)
		printf("%s\n", cluster->data);

	else
		printf("Arquivo não encontrado\n");
}


//Função que junta todas as outras em um simples shell
void shell(void)
{
	char nome[4096] = { 0 };
    char nomeAux[4096] = { 0 };
	char copiaDoNome[4096] = { 0 };
	const char aux[2] = "/";
	char aux2[4096] = { 0 };

	char *comandoDigitado;
	int i;

	printf("Digite o comando: ");
	fgets(nome,4096,stdin);

	strcpy(copiaDoNome,nome);

	comandoDigitado = strtok(nome,aux);

	if ( strcmp(comandoDigitado, "init\n") == 0)
	{
		init();
	}
	else if ( strcmp(comandoDigitado, "load\n") == 0)
	{
		carregaFat();
	}
	else if ( strcmp(comandoDigitado, "ls ") == 0 && copiaDoNome[3] == '/') 
	{
		for(i = 3; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-3] = copiaDoNome[i];
		}
		ls(aux2);
	}
	else if ( strcmp(comandoDigitado, "mkdir ") == 0 && copiaDoNome[6] == '/')
	{
		for(i = 6; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-6] = copiaDoNome[i];
		}
		mkdir(aux2);
	}
	else if ( strcmp(comandoDigitado, "create ") == 0 && copiaDoNome[7] == '/')
	{
		for(i = 7; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-7] = copiaDoNome[i];
		}
		create(aux2);
	}
		else if ( strcmp(comandoDigitado, "unlink ") == 0 && copiaDoNome[7] == '/')
	{
		for(i = 7; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-7] = copiaDoNome[i];
		}
		unlink(aux2);
	}
	else if ( strcmp(comandoDigitado, "write ") == 0 && copiaDoNome[6] == '/')
	{
		for(i = 6; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-6] = copiaDoNome[i];
		}
		printf("Digite o texto:\n");
		fgets(nomeAux,4096,stdin);
		write(aux2,nomeAux);
	}
	else if ( strcmp(comandoDigitado, "append ") == 0 && copiaDoNome[7] == '/')
	{
		for(i = 7; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-7] = copiaDoNome[i];
		}
		printf("Digite o texto");
		fgets(nomeAux,4096,stdin);
		append(aux2,nomeAux);
	}
	else if ( strcmp(comandoDigitado, "read ") == 0 && copiaDoNome[5] == '/')
	{
		for(i = 5; i < strlen(copiaDoNome)-1; ++i)
		{
			aux2[i-5] = copiaDoNome[i];
		}
		read(aux2);
	}

	
	else printf("Erro no comando \n");
}

void comandoInicializar()
{
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
