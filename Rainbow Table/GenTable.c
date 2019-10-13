#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <math.h>
#include <string.h>
#include <time.h>



int main (int argc,char **argv){

	int n = atoi(argv[1]);
	int seed = atoi(argv[2]);
	int i, j, k, s, outl, l;
	unsigned char password1 [16];
	unsigned char ciphertext[16];
	char hello = 0x00;
	char plaintext [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	unsigned char iv[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	int pow = 1; 
	for(i = 0; i<n/2; i++)
		pow = 2*pow;
	srand(seed);
	
	
	FILE *ptr = fopen("rainbow1", "w");
	if(ptr == NULL){
	 	printf("ERROR");
	 	return 0;
	 }

	for(i = 0; i<pow; i++){ 
		 //We fill the first 128-n positions with zeros
		for(j = 0; j<16-(n/8)-((n%8)/4); j++)
			password1[j] = 0x00;

		//Generate the random n bits

		if(n % 8 !=0){
			char str[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
		    password1[15-(n/8)] = str[rand() % 16]; 
		}

		for(k = 16-(n/8); k<16; k++){
		 	password1[k] = 'A' + rand()%26;
		}



		 for(j = 0; j<sizeof(password1); j++)
		 	fprintf(ptr, "%02x", password1[j]);
		 fprintf(ptr, "\n");


		for(k = 0; k<pow; k++){
			EVP_CIPHER_CTX *ctx;
			ctx = EVP_CIPHER_CTX_new();
			EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, password1, iv);
			EVP_EncryptUpdate(ctx, ciphertext, &outl, plaintext, 32);
			EVP_CIPHER_CTX_free(ctx);
			

			//REDUCTION FUNCTION!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

			for(j = 0; j<16; j++){
				//for(s = 0; s<j; s++)
					//hello+=ciphertext[s];
				ciphertext[j] = ciphertext[j]%pow; 
				hello = 0x00;  
			}
		    for(j = 0; j<16-(n/8)-((n%8)/4); j++)
				ciphertext[j] = 0x00;
			if((n%8)!=0)
				ciphertext[15-(n/8)] = ciphertext[15-(n/8)] & 0x0F;
			


			for(j = 0; j<16; j++)
				password1[j] = ciphertext[j];
		}

		for(j = 0; j<sizeof(ciphertext); j++)
		 	fprintf(ptr, "%02x", ciphertext[j]);
		 
		 fprintf(ptr, "\n");
	}


	 // for(i = 0; i<sizeof(ciphertext); i++)
	 // 	printf("%02x", ciphertext[i]);
	 // printf("\n");

	 // for(i = 0; i<sizeof(plaintext); i++)
	 // 	printf("%02x", plaintext[i]);
	 // printf("\n");

	 

	 fclose(ptr);

	return 0;

}