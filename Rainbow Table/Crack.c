#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <math.h>
#include <string.h>
#include <time.h>

unsigned char endpoint[16];

void String_to_Hex(char* password1){
	char aux[2];
	int i;
	
	for(i = 0; i<16; i++){
		aux[0] = password1[2*i];
		aux[1] = password1[2*i+1];
		endpoint[i] = (int)strtol(aux, NULL, 16);
	}
}

int main (int argc,char **argv){

	if(argc !=3){
		printf("ERROR");
		return 0;
	}

	int n = atoi(argv[1]);
	int i, j, k,outl;
	unsigned char hash[16];
	
	unsigned char newHash[16];
	unsigned char auxHash[16];
	unsigned char hashCopy[16];
	unsigned char password[32];
	unsigned char red_Copy[16];
	unsigned char finalPassword[16];
	unsigned char aux[2];
	unsigned char hello = 0x00;
	char plaintext [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	unsigned char iv[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int a = 0, b = 0;

	for(i = 0; i<16; i++){
		aux[0] = argv[2][2*i];
		aux[1] = argv[2][2*i+1];
		hash[i] = (int)strtol(aux, NULL, 16);
		//printf("%02x", hash[i]);
	}
	
	//printf("\n");

	FILE *ptr = fopen("rainbow1", "r");
	if(ptr == NULL){
	 	printf("ERROR");
	 	return 0;
	 }

	for(j = 0; j<16; j++){
		hashCopy[j] = hash[j];
	}

	for(i = 0; i<16; i++)
				  	printf("%02x", hashCopy[i]);
				  printf("\n");

	//aux variables: l=point in the hash; m=chain number
	int l = 0;
	int m = 0;
	int pow = 1; 
	for(i = 0; i<atoi(argv[1])/2; i++){
		pow = pow*2;
	}
	printf("%d\n", pow);

	while(a==0 && l<pow){
		m = 0;
		while(a==0 && m<pow){
				//Compare with all the endpoints
				if(b==0){
					fseek(ptr, 33, SEEK_SET);
					b++;
				}else{
					fseek(ptr,34,SEEK_CUR);
				}
				fread(password, 32, 1, ptr);

				String_to_Hex(password);

				  // for(i = 0; i<16; i++)
				  // 	printf("%02x", hashCopy[i]);
				  // printf("\n");

				  // for(i = 0; i<16; i++)
				  // 	printf("%02x", endpoint[i]);

				  // printf("\n");

				EVP_CIPHER_CTX *ctx;
				ctx = EVP_CIPHER_CTX_new();
				EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, endpoint, iv);
				EVP_EncryptUpdate(ctx, newHash, &outl, plaintext, 32);
				EVP_CIPHER_CTX_free(ctx);
				
				 if(strcmp(newHash, hashCopy) == 0){
				 	a++;
				 	printf("Password found!!!\n");
				 	// for(i = 0; i<16; i++)
				  // 	printf("%02x", hashCopy[i]);
				  // 	printf("\n");

				  // 	for(i = 0; i<16; i++)
				  // 	printf("%02x", newHash[i]);

				  // 	printf("\n");

				  	for(j = 0; j<16; j++)
						finalPassword[j] = endpoint[j];

				 }
				 m++;
			}
		
		b = 0;

		if(a!=0){
			//REDUCTION !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			for(j = 0; j<16; j++){
				//for(k = 0; k<j; k++)
					//hello+=hashCopy[k];
				hashCopy[j] = hashCopy[j]%pow; 
				hello = 0x00;
			}
		    for(j = 0; j<16-(n/8)-((n%8)/4); j++){
				hashCopy[j] = 0x00;
		    }
			if((n%8)!=0){
				hashCopy[15-(n/8)] = hashCopy[15-(n/8)] & 0x0F;
			}

			//Encrypt
			EVP_CIPHER_CTX *ctx;
			ctx = EVP_CIPHER_CTX_new();
			EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, hashCopy, iv);
			EVP_EncryptUpdate(ctx, auxHash, &outl, plaintext, 32);
			EVP_CIPHER_CTX_free(ctx);

			for(j = 0; j<16; j++)
				hashCopy[j] = auxHash[j];
		}
		l++;
}



	printf("\n %d   %d \n", m, l);
	
	if(a!=0){
		printf("The password is: ");
		for(i = 0; i<16; i++)
			printf("%02x", finalPassword[i]);
	} else{
		printf("NO PASSWORD FOUND");
	}

	printf("\n");
	fclose(ptr);

}