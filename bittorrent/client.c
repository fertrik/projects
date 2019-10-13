#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <argp.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <math.h>
#include <unistd.h>


#include "bencode.h"
#include "hash.h"


struct peer{
	long port;
	char *ip;
	int idx;
	char* id;
};

struct request_stuff{
	int* my_Bitfield;
	int socket;
	int idx;
};

struct peer *peer_arr = NULL;
uint8_t handshake[68];
int downloaded = 0, num_pieces = 0, num_subPieces, last_size, final_num_peers;
int *bitfield, *choke_array, **num_subpieces_array;
uint8_t ***piecefield;
char *piece_to_hash;
pthread_mutex_t lock;
be_node_t *pieces;
pthread_t *thread_array;

void hash(char* outBuf, int outBufLen, uint8_t checksum[20]){
	int i;
	struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
	if (!ctx) {
		fprintf(stderr, "Error creating checksum\n");
		//return 0;
	}

	unsigned char info_hash[outBufLen];
	for(i = 0; i < outBufLen; i++){
		info_hash[i] = (unsigned) outBuf[i];
	}


	sha1sum_update(ctx, info_hash, outBufLen);
	sha1sum_finish(ctx, info_hash, 0, checksum);

	// printf("Hash: 0x");
	// for(i = 0; i < 20; ++i) {
	// 	printf("%02x", checksum[i]);
	// }
	// putchar('\n');

	sha1sum_destroy(ctx);

	//return checksum;
}

void *request(void *arg){

	struct request_stuff req_stf = *(struct request_stuff*) arg;
	uint8_t *reqBuff = (uint8_t*) calloc(17, sizeof(uint8_t));
	int i, j;
	//int a = floor(num_subPieces/3);
	uint32_t index, offset, max_offset = 0, last_bits;
	reqBuff[3] = 0x0d;
	reqBuff[4] = 0x06;
	reqBuff[15] = 0x40;
	while(choke_array[req_stf.idx] == 1){
		for(i = 0; i<num_pieces; i++){
			if(choke_array[req_stf.idx] == 0){
				pthread_exit(NULL);
			}
		    if(downloaded == 1){
		        pthread_exit(NULL);
		    }
			else if((req_stf.my_Bitfield[i] == 1) && (bitfield[i] == 1)){//send

				pthread_mutex_lock(&lock);
				while(bitfield[i] == 2) i++;
				bitfield[i] = 2;
				//printf("I am %i and my num_piece is %i\n", req_stf.idx, i);
				pthread_mutex_unlock(&lock);

		        if(downloaded == 1){
		          pthread_exit(NULL);
		        }

				index = i;
				offset = 0;
				reqBuff[5] = (index & 0xff000000)>>24;
				reqBuff[6] = (index & 0x00ff0000)>>16;
				reqBuff[7] = (index & 0x0000ff00)>>8;
				reqBuff[8] = (index & 0x000000ff);
				if(index == num_pieces-1){
					max_offset = last_size/16384;
					for(j = 0; j<max_offset; j++){
						reqBuff[9] = (offset & 0xff000000)>>24;
						reqBuff[10] = (offset & 0x00ff0000)>>16;
						reqBuff[11] = (offset & 0x0000ff00)>>8;
						reqBuff[12] = (offset & 0x000000ff);
						offset += 0x00004000;
						if(send(req_stf.socket, reqBuff, 17, 0) != 17){
							printf("id: %i->Send failed in request function\n", req_stf.idx);
							if(downloaded == 1){
								pthread_exit(NULL);
							}
							pthread_t thread;
							pthread_create(&thread, NULL, request, &req_stf);
							pthread_join(thread, NULL);
							pthread_exit(NULL);
						}

					  if(j%4 == 0) usleep(200000);
					}
					reqBuff[9] = (offset & 0xff000000)>>24;
					reqBuff[10] = (offset & 0x00ff0000)>>16;
					reqBuff[11] = (offset & 0x0000ff00)>>8;
					reqBuff[12] = (offset & 0x000000ff);
					last_bits = last_size - offset;
					reqBuff[13] = (last_bits & 0xff000000)>>24;
					reqBuff[14] = (last_bits & 0x00ff0000)>>16;
					reqBuff[15] = (last_bits & 0x0000ff00)>>8;
					reqBuff[16] = (last_bits & 0x000000ff);

					if(send(req_stf.socket, reqBuff, 17, 0) != 17){
						printf("id: %i->Send failed in request function\n", req_stf.idx);
						if(downloaded == 1){
							pthread_exit(NULL);
						}
						pthread_t thread;
						pthread_create(&thread, NULL, request, &req_stf);
						pthread_join(thread, NULL);
						pthread_exit(NULL);
					}
					reqBuff[13] = 0x00;
					reqBuff[14] = 0x00;
					reqBuff[15] = 0x40;
					reqBuff[16] = 0x00;

				  //usleep(40000);

				}else{
					for(j = 0; j<num_subPieces; j++){
						reqBuff[9] = (offset & 0xff000000)>>24;
						reqBuff[10] = (offset & 0x00ff0000)>>16;
						reqBuff[11] = (offset & 0x0000ff00)>>8;
						reqBuff[12] = (offset & 0x000000ff);
						offset += 0x00004000;
						if(send(req_stf.socket, reqBuff, 17, 0) != 17){
							printf("id: %i->Send failed in request function\n", req_stf.idx);
							if(downloaded == 1){
								pthread_exit(NULL);
							}
							pthread_t thread;
							pthread_create(&thread, NULL, request, &req_stf);
							pthread_join(thread, NULL);
							pthread_exit(NULL);
						}
						//usleep(40000);
						if(j%4 == 0) usleep(200000);
					}
				}
			}
			//usleep(40000);
		}
		printf("I'm %i and I enter the second FOR loop\n", req_stf.idx);
		while(downloaded == 0){
			for(i = 0; i<num_pieces; i++){
				if(choke_array[req_stf.idx] == 0){
					pthread_exit(NULL);
				}
				if(downloaded == 1){
					pthread_exit(NULL);
				}
				else if((req_stf.my_Bitfield[i] == 1) && (bitfield[i] == 2)){//send

					index = i;
					offset = 0;
					reqBuff[5] = (index & 0xff000000)>>24;
					reqBuff[6] = (index & 0x00ff0000)>>16;
					reqBuff[7] = (index & 0x0000ff00)>>8;
					reqBuff[8] = (index & 0x000000ff);
					if(index == num_pieces-1){
						max_offset = last_size/16384;
						for(j = 0; j<max_offset-1; j++){
							reqBuff[9] = (offset & 0xff000000)>>24;
							reqBuff[10] = (offset & 0x00ff0000)>>16;
							reqBuff[11] = (offset & 0x0000ff00)>>8;
							reqBuff[12] = (offset & 0x000000ff);
							offset += 0x00004000;
							if(send(req_stf.socket, reqBuff, 17, 0) != 17){
								printf("id: %i->Send failed in request function\n", req_stf.idx);
								if(downloaded == 1){
									pthread_exit(NULL);
								}
								pthread_t thread;
								pthread_create(&thread, NULL, request, &req_stf);
								pthread_join(thread, NULL);
								pthread_exit(NULL);
							}
							if(j%4 == 0) usleep(200000);
						}
						reqBuff[9] = (offset & 0xff000000)>>24;
						reqBuff[10] = (offset & 0x00ff0000)>>16;
						reqBuff[11] = (offset & 0x0000ff00)>>8;
						reqBuff[12] = (offset & 0x000000ff);
						last_bits = (uint32_t)last_size - offset;
						reqBuff[13] = (last_bits & 0xff000000)>>24;
						reqBuff[14] = (last_bits & 0x00ff0000)>>16;
						reqBuff[15] = (last_bits & 0x0000ff00)>>8;
						reqBuff[16] = (last_bits & 0x000000ff);

						if(send(req_stf.socket, reqBuff, 17, 0) != 17){
							printf("id: %i->Send failed in request function\n", req_stf.idx);
							if(downloaded == 1){
								pthread_exit(NULL);
							}
							pthread_t thread;
							pthread_create(&thread, NULL, request, &req_stf);
							pthread_join(thread, NULL);
							pthread_exit(NULL);
						}

						reqBuff[13] = 0x00;
						reqBuff[14] = 0x00;
						reqBuff[15] = 0x40;
						reqBuff[16] = 0x00;
						//usleep(40000);

					}else{
						for(j = 0; j<num_subPieces; j++){
							reqBuff[9] = (offset & 0xff000000)>>24;
							reqBuff[10] = (offset & 0x00ff0000)>>16;
							reqBuff[11] = (offset & 0x0000ff00)>>8;
							reqBuff[12] = (offset & 0x000000ff);
							offset += 0x00004000;
							if(send(req_stf.socket, reqBuff, 17, 0) != 17){
								printf("id: %i->Send failed in request function\n", req_stf.idx);
								if(downloaded == 1){
									pthread_exit(NULL);
								}
								pthread_t thread;
								pthread_create(&thread, NULL, request, &req_stf);
								pthread_join(thread, NULL);
								pthread_exit(NULL);

							}
							//usleep(40000);
							if(j%4 == 0) usleep(200000);
						}
					}
				}
				//usleep(40000);
			}
		}
	}
	if(downloaded == 0){
    	pthread_t thread;
    	pthread_create(&thread, NULL, request, &req_stf);
    	pthread_join(thread, NULL);
	}
	if(downloaded == 1){
		pthread_exit(NULL);
	}
	pthread_exit(NULL);
}



void *function(void *arg){

	if(downloaded == 1)
		pthread_exit(NULL);

	struct peer my_peer = *(struct peer*) arg;

	int sock, i;
	struct sockaddr_in server_addr;

	if((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		printf("Socket failed\n");
		pthread_exit(NULL);
	}

	//Hay que hacer un metodo que me corte bttracker.debian.org

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(my_peer.ip);
	server_addr.sin_port = htons(my_peer.port);


	if(connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0 || downloaded == 1){
		printf("id: %i->Connect() failed\n", my_peer.idx);
		pthread_exit(NULL);
	}

	printf("id: %i->Connected successfully\n", my_peer.idx);

	if(send(sock, handshake, 68, 0) != 68){
		printf("id: %i->Send failed\n", my_peer.idx);
		pthread_exit(NULL);
	}

	uint8_t *recv_Buffer = (uint8_t*) malloc(68);
	int bytes_rcv;

	if ((bytes_rcv = recv(sock, recv_Buffer, 68, 0)) <= 0){
		printf("id: %i->Receiving failed\n", my_peer.idx);
		pthread_exit(NULL);
	}

	//printf("%i received %i bytes\n", my_peer.idx, bytes_rcv);

	peer_arr[my_peer.idx].id = (char*) malloc(20);
	for(i = 0; i<20; i++){
		peer_arr[my_peer.idx].id[i] = recv_Buffer[48+i];
	}

	//printf("Peer id = %s\n", peer_arr[my_peer.idx].id);

	int length = 0;
	int *my_Bitfield = (int*) calloc(num_pieces, sizeof(int));
	uint8_t *sendBuff_5 = (uint8_t*) malloc(5);
	uint8_t *checksum = (uint8_t*) malloc(20);
	sendBuff_5[0] = 0x00;
	sendBuff_5[1] = 0x00;
	sendBuff_5[2] = 0x00;
	sendBuff_5[3] = 0x01;
	int index = 0, offset = 0, offset_idx = 0, max_offset = last_size/16384, j, pieces_aux, flg = 0, flg2 = 0;
	struct request_stuff request_stf;

	int auxiliar_rcv_size;
	uint8_t *aux_read = (uint8_t*)malloc(1*sizeof(uint8_t));
	uint8_t *aux_hash = (uint8_t*)malloc(20);


	while(downloaded == 0){

		recv_Buffer = (uint8_t*) realloc(recv_Buffer, 4);
		usleep(10000);
		if ((bytes_rcv = recv(sock, recv_Buffer, 4, 0)) <= 0){
			printf("id: %i->Receiving failed for length\n", my_peer.idx);
			if(downloaded == 1){
				pthread_exit(NULL);
			}
			pthread_t thread2;
			pthread_create(&thread2, NULL, function, &peer_arr[my_peer.idx]);
			pthread_join(thread2, NULL);
			pthread_exit(NULL);
		}


		for(i = 0; i<4; i++){
			recv_Buffer[i] = recv_Buffer[i] & 0xff;
		}

		length = pow(256,3)*(unsigned int)recv_Buffer[0] + pow(256,2)*(unsigned int)recv_Buffer[1] +
		pow(256,1)*(unsigned int)recv_Buffer[2] + (unsigned int)recv_Buffer[3];
		//printf("Length = %i", length);
		flg = 0;
		if(length == 0){
			flg = 1;
		} else if(length > 16393){
      flg = 2;
    }
		if(flg == 1){
			printf("keep-alive by %s   ", my_peer.ip);//keep-alive
			printf("%02x %02x %02x %02x\n", recv_Buffer[0],recv_Buffer[1],recv_Buffer[2],recv_Buffer[3]);
			if(downloaded == 1)
				pthread_exit(NULL);
		}else if(flg == 2){

    }else{
			recv_Buffer = (uint8_t*) realloc(recv_Buffer, 1);

			if ((bytes_rcv = recv(sock, recv_Buffer, 1, 0)) <= 0){
				printf("id: %i->Receiving failed for type\n", my_peer.idx);
				if(downloaded == 1){
					pthread_exit(NULL);
				}
				pthread_t thread;
				pthread_create(&thread, NULL, function, &peer_arr[my_peer.idx]);
				pthread_join(thread, NULL);
				pthread_exit(NULL);
			}
			//printf("   Type: %02x\n", recv_Buffer[0]);



			if(recv_Buffer[0] == 0x05){//bitField
				recv_Buffer = (uint8_t*) realloc(recv_Buffer, length-1);

				if ((bytes_rcv = recv(sock, recv_Buffer, length-1, 0)) <= 0){
					printf("id: %i->Receiving failed\n", my_peer.idx);
					pthread_exit(NULL);
				}

				if(bytes_rcv != length-1){
					auxiliar_rcv_size = bytes_rcv;
					aux_read = (uint8_t*)realloc(aux_read, length-1-auxiliar_rcv_size);


					if ((bytes_rcv = recv(sock, aux_read, length-1-auxiliar_rcv_size, 0)) <= 0){
						printf("id: %i->Receiving failed\n", my_peer.idx);
						pthread_exit(NULL);
					}

					for(i = 0; i < length-1-auxiliar_rcv_size; i++){
						recv_Buffer[i+auxiliar_rcv_size] = aux_read[i];
					}
				}

				for(i = 0; i<length-1; i++){
					if((recv_Buffer[i] & 0x80)==0x80){
			            if(bitfield[8*i] == 0){
			              bitfield[8*i] = 1;
			            }
						my_Bitfield[8*i] = 1;
					}
					if((recv_Buffer[i] & 0x40)==0x40){
			            if(bitfield[8*i+1] == 0){
			              bitfield[8*i+1] = 1;
			            }
						my_Bitfield[8*i+1] = 1;
					}
					if((recv_Buffer[i] & 0x20)==0x20){
			            if(bitfield[8*i+2] == 0){
			              bitfield[8*i+2] = 1;
			            }
						my_Bitfield[8*i+2] = 1;
					}
					if((recv_Buffer[i] & 0x10)==0x10){
			            if(bitfield[8*i+3] == 0){
			              bitfield[8*i+3] = 1;
			            }
						my_Bitfield[8*i+3] = 1;
					}
					if((recv_Buffer[i] & 0x08)==0x08){
            			if(bitfield[8*i+4] == 0){
              				bitfield[8*i+4] = 1;
            			}
						my_Bitfield[8*i+4] = 1;

					}
					if((recv_Buffer[i] & 0x04)==0x04){
			            if(bitfield[8*i+5] == 0){
			              bitfield[8*i+5] = 1;
			            }
						my_Bitfield[8*i+5] = 1;
					}
					if((recv_Buffer[i] & 0x02)==0x02){
			            if(bitfield[8*i+6] == 0){
			              bitfield[8*i+6] = 1;
			            }
						my_Bitfield[8*i+6] = 1;
					}
					if((recv_Buffer[i] & 0x01)==0x01){
			            if(bitfield[8*i+7] == 0){
			              bitfield[8*i+7] = 1;
			            }
						my_Bitfield[8*i+7] = 1;
					}
				}

				//Send interested
				sendBuff_5[4] = 0x02;
				if(send(sock, sendBuff_5, 5, 0) != 5){
					printf("id: %i->Send failed\n", my_peer.idx);
					pthread_exit(NULL);
				}

			}else if(recv_Buffer[0] == 0x04){//have
				recv_Buffer = (uint8_t*) realloc(recv_Buffer, length-1);

				if ((bytes_rcv = recv(sock, recv_Buffer, length-1, 0)) <= 0){
					printf("id: %i->Receiving failed HAVE\n", my_peer.idx);
					if(downloaded == 1){
						pthread_exit(NULL);
					}
					pthread_t thread;
					pthread_create(&thread, NULL, function, &peer_arr[my_peer.idx]);
					pthread_join(thread, NULL);
					pthread_exit(NULL);
				}

				index = pow(256,3)*(unsigned int)recv_Buffer[0] + pow(256,2)*(unsigned int)recv_Buffer[1] +
				pow(256,1)*(unsigned int)recv_Buffer[2] + (unsigned int)recv_Buffer[3];

				if(bitfield[index] == 0) bitfield[index] = 1;
				my_Bitfield[index] = 1;
			}else if(recv_Buffer[0] == 1){//unchoke
				choke_array[my_peer.idx] = 1;
				request_stf.my_Bitfield = my_Bitfield;
				request_stf.socket = sock;
				request_stf.idx = my_peer.idx;
				pthread_t thread;
				pthread_create(&thread, NULL, request, &request_stf);
			}else if(recv_Buffer[0] == 0){//choke
				choke_array[my_peer.idx] = 0;
			}else if(recv_Buffer[0] == 7){//piece
				recv_Buffer = (uint8_t*) realloc(recv_Buffer, length-1);

				usleep(150000);
				if ((bytes_rcv = recv(sock, recv_Buffer, length-1, 0)) <= 0){
					printf("id: %i->Receiving failed PIECE\n", my_peer.idx);
					if(downloaded == 1){
						pthread_exit(NULL);
					}
					pthread_t thread;
					pthread_create(&thread, NULL, function, &peer_arr[my_peer.idx]);
					pthread_join(thread, NULL);
					pthread_exit(NULL);
				}
				while(bytes_rcv != length-1){
					auxiliar_rcv_size = bytes_rcv;
					aux_read = (uint8_t*)realloc(aux_read, length-1-auxiliar_rcv_size);
					//printf("Need to receive more bytes, received just: %i\n\n", auxiliar_rcv_size);
					usleep(150000);
					if ((bytes_rcv = recv(sock, aux_read, length-1-auxiliar_rcv_size, 0)) <= 0){
						printf("id: %i->Receiving failed\n", my_peer.idx);
						if(downloaded == 1){
							pthread_exit(NULL);
						}
						pthread_t thread;
						pthread_create(&thread, NULL, function, &peer_arr[my_peer.idx]);
						pthread_join(thread, NULL);
						pthread_exit(NULL);
					}
					//printf("Bytes rcv are: %i \n", (auxiliar_rcv_size+bytes_rcv));

					for(i = 0; i < length-1-auxiliar_rcv_size; i++){
						recv_Buffer[i+auxiliar_rcv_size] = aux_read[i];
					}
				}

				index = pow(256,3)*(unsigned int)recv_Buffer[0] + pow(256,2)*(unsigned int)recv_Buffer[1] +
				pow(256,1)*(unsigned int)recv_Buffer[2] + (unsigned int)recv_Buffer[3];

				offset = pow(256,3)*(unsigned int)recv_Buffer[4] + pow(256,2)*(unsigned int)recv_Buffer[5] +
				pow(256,1)*(unsigned int)recv_Buffer[6] + (unsigned int)recv_Buffer[7];

				offset_idx = offset/16384;

				if(bitfield[index] == 2){

					for(i = 0; i<16384; i++){
						piecefield[index][offset_idx][i] = recv_Buffer[i+8];
					}
					for(i = 0; i<16384; i++){
						piecefield[index][offset_idx][i] = piecefield[index][offset_idx][i];
					}
					num_subpieces_array[index][offset_idx] = 1;
					//printf("Index: %i, %i\n", index, offset_idx);

				}
				pieces_aux = 0;
				for(i = 0; i<num_subPieces; i++){
					if(num_subpieces_array[index][i] == 1)
						pieces_aux++;
				}
				if(pieces_aux == num_subPieces && index!=num_pieces-1){

					for(i = 0; i<num_subPieces; i++){
						for(j = 0; j<16384; j++){
							piece_to_hash[16384*i + j] = piecefield[index][i][j];
						}
					}

        			if(downloaded == 1){
            			pthread_exit(NULL);
        			}

					hash(piece_to_hash, num_subPieces*16384, checksum);

					for(i = 0; i<20; i++){
						aux_hash[i] = pieces->x.str.buf[20*index+i];
					}

					if(memcmp(checksum, aux_hash, 20) == 0){
						if(bitfield[index] != 3)
							printf("Piece %i downloaded by %i\n", index, my_peer.idx);
						if(downloaded == 1)
							pthread_exit(NULL);
						bitfield[index] = 3;
						for(i = 0; i<num_pieces; i++){
							if(bitfield[i] == 3)
								flg2++;
						}
						//printf("\nFLAG == %i\n", flg2);
						if(flg2 == num_pieces){
							printf("\n\n\n\nALL PIECES DOWNLOADED\n\n\n\n");
							downloaded = 1;
							for(i = 0; i < final_num_peers; i++){
								if(i !=my_peer.idx){
									pthread_cancel(thread_array[i]);
								}
							}
							pthread_exit(NULL);
						}
						flg2 = 0;
					}else{
						for(j = 0; j<num_subPieces; j++){
							num_subpieces_array[index][j] = 0;
						}
					}

				}else if(pieces_aux == max_offset+1 && index==num_pieces-1){

					for(i = 0; i<max_offset; i++){
						for(j = 0; j<16384; j++){
							piece_to_hash[16384*i + j] = piecefield[index][i][j];
						}
					}
					for(j = 0; j<(last_size-(16384*max_offset)); j++){
						piece_to_hash[16384*i + j] = piecefield[index][i][j];
					}


					hash(piece_to_hash, last_size, checksum);

					for(i = 0; i<20; i++){
						aux_hash[i] = pieces->x.str.buf[20*index+i];
						//printf("%02x ", aux_hash[i]);
					}
					if(memcmp(checksum, aux_hash, 20) == 0){
						printf("Piece %i, downloaded by %i\n", index, my_peer.idx);
						if(downloaded == 1)
							pthread_exit(NULL);
						bitfield[index] = 3;
						for(i = 0; i<num_pieces; i++){
							if(bitfield[i] == 3)
								flg2++;
						}
						if(flg2 == num_pieces-1){
				            printf("\n\n\n\nALL PIECES DOWNLOADED\n\n\n\n");
				            downloaded = 1;
				            //kill all the threads with pthread_cancel
							for(i = 0; i < final_num_peers; i++){
								if(i !=my_peer.idx){
									pthread_cancel(thread_array[i]);
								}
							}
			            	pthread_exit(NULL);
            			}
						flg2 = 0;
					}
				}
			}
		}
	}
	pthread_exit(NULL);
}



struct in_addr getTrackerIP(char *hostname){
	struct hostent *he;
	struct in_addr result;

	//printf("The hostname is: %s \n", hostname);

	he = gethostbyname(hostname);
	//printf("Hostname: %s\n", he->h_name);
 	//printf("Address type #: %d\n", he->h_addrtype);
 	//printf("Address length: %d\n", he->h_length);

	if (!he)
	{
		// get the host info
		herror("gethostbyname");
		exit(1);
	}
 	result= *(struct in_addr *)(he->h_addr);
   printf("Hostname: %s, was resolved to: %s\n",
           hostname,inet_ntoa(result));


return result;

}



char* url_enc(uint8_t checksum[20]){
	int str_hash_length = 0;
	char info_hash[60];
	int it, i;
	for(i = 0; i<20; i++){
		if((checksum[i]>0x40 && checksum[i]<0x7b) || (checksum[i] == 0x2d) ||
		(checksum[i] == 0x2e) || (checksum[i] == 0x7e) || (checksum[i] == 0x5f)){
			char str_hash[2];
			str_hash_length += 1;
			sprintf(str_hash, "%c", checksum[i]);
			info_hash[str_hash_length-1] = str_hash[0];
		}else{
			char str_hash[4];
			str_hash_length += 3;
			sprintf(str_hash, "%%%02x", checksum[i]);
			for(it = 0; it < 4; it++){
				info_hash[str_hash_length-3+it] = str_hash[it];

				if(info_hash[str_hash_length-3+it] > 0x60 && info_hash[str_hash_length-3+it] < 0x7b){
					info_hash[str_hash_length-3+it] -=32;
				}
			}
		}
	}
	char* encoded = (char*) malloc(str_hash_length);
	strcpy(encoded,info_hash);

	return encoded;
}



int main(int argc, char **argv){

	if(downloaded == 1)
		pthread_exit(NULL);

	FILE *torrent;
	be_node_t *node;
	size_t rx, len;
	int i, found, end, j;
	time_t t;//for random
	char user_in[150];


	printf("Type the file name to download: ");
	scanf("%s", user_in);
	torrent = fopen(user_in, "r");
	fseek(torrent, 0, SEEK_END);
	len = ftell(torrent);


	char* encoded_str = (char*) malloc(len);
	fseek(torrent, 0, SEEK_SET);
	fread(encoded_str, len, 1, torrent);



	//printf("%s\n", encoded_str);

	node = be_decode(encoded_str, len, &rx);
    BE_ASSERT(node != NULL);

    if(encoded_str != NULL){
    	free(encoded_str);
    	encoded_str = NULL;
    }

    be_node_t *info = be_dict_lookup(node, "info", NULL);//info is a dictionary
    be_node_t *announce = be_dict_lookup(node, "announce", NULL);//announce is a string



    be_node_t *length = be_dict_lookup(info, "length", NULL);
    be_node_t *piece_length = be_dict_lookup(info, "piece length", NULL);
		be_node_t *name_for_file = be_dict_lookup(info, "name", NULL);
    pieces = be_dict_lookup(info, "pieces", NULL);

    num_pieces = length->x.num/piece_length->x.num + 1;

    last_size = length->x.num-(piece_length->x.num*(num_pieces-1));
    //printf("Last size = %i\n", last_size);

    bitfield = (int*) calloc(num_pieces, sizeof(int));

    num_subPieces = piece_length->x.num/16384;

    num_subpieces_array = (int**) malloc(num_pieces*sizeof(int*));
    for(i = 0; i<num_pieces; i++){
    	num_subpieces_array[i] = (int*) calloc(num_subPieces, sizeof(int));
    }



    piece_to_hash = (char*) malloc(piece_length->x.num);

    piecefield = (uint8_t***) malloc(num_pieces * sizeof(uint8_t**));
    for(i = 0; i<num_pieces; i++){
    	piecefield[i] = (uint8_t**) malloc(num_subPieces*sizeof(uint8_t*));
    	for(j = 0; j<num_subPieces; j++){
    		piecefield[i][j] = (uint8_t*) calloc(16384, sizeof(uint8_t));
    	}
    }

    char* tracker_url = (char*) malloc (announce->x.str.len);
    memcpy(tracker_url, announce->x.str.buf, announce->x.str.len);
    printf("Tracker URL: %s\n", tracker_url);


    for(i = 0; i<strlen(tracker_url); i++){
    	if(tracker_url[i] == '/' && tracker_url[i+1] == '/'){
    		found = i+2;
    	}
    }

    i = found;
    while(tracker_url[i] != '/'){
    	i++;
    }
    end = i;

    char *url = (char*) malloc(end-found);
    for(i = found; i<end; i++){
    	url[i-found] = tracker_url[i];
    }
   url[i-found] = '\0';
    printf("URL = %s\n", url);


    //Me quiero sacar el puerto. Hace falta para tcp. es un poco guarrada, pero funciona...
    // char *token;
    // token = strtok_r(announce->x.str.buf, ":", &announce->x.str.buf);
    // token = strtok_r(announce->x.str.buf, ":", &announce->x.str.buf);
    // token = strtok_r(announce->x.str.buf, "/", &announce->x.str.buf);
    // int port = atoi(token);
    int port = 80;

    if(tracker_url != NULL){
    	free(tracker_url);
    	tracker_url = NULL;
    }



    //Build the HTTP message (get announce + hash)

	uint8_t *http_get = (uint8_t*)malloc(sizeof(uint8_t)*13);


	//FOR GET /: 47 45 54 20 2f
	 http_get[0] = 0x47; http_get[1] = 0x45; http_get[2] = 0x54; http_get[3] = 0x20; http_get[4] = 0x2f;
	// //Then we have the announce? 61 6e 6e 6f 75 6e 63 65 3f
	 http_get[5] = 0x61; http_get[6] = 0x6e; http_get[7] = 0x6e; http_get[8] = 0x6f; http_get[9] = 0x75;
	 http_get[10] = 0x6e; http_get[11] = 0x63; http_get[12] = 0x65;

	//Encode info para poder hashear
	ssize_t encode_size;
	size_t outBufLen = 1;
	char *outBuf = NULL;

	encode_size = be_encode(info, outBuf, outBufLen);
	if(outBuf == NULL){
		outBufLen = encode_size;
		outBuf = (char*)realloc(outBuf, outBufLen);
		encode_size = be_encode(info, outBuf, outBufLen);
	}

	//printf("Size of outBufLen: %ld \n", outBufLen);

	uint8_t *checksum = (uint8_t*) malloc(20);
	hash(outBuf, outBufLen, checksum);


	//Infohash url encoding
	char* info_hash_encoded = url_enc(checksum);


	//printf("URL encoded hash outside: %s\n", info_hash_encoded);

	//peer-id generation
	srand((unsigned) time(&t));
	uint8_t *peer_id = (uint8_t*) malloc(4);
	// for(i = 0; i<20; i++){
	// 	peer_id[i] = rand() % 256;
	// 	///printf("0x%02x ", peer_id[i]);
	// }
	peer_id[0] = 'a';
	peer_id[1] = 'b';
	peer_id[2] = 'c';
	peer_id[3] = 'd';
	//puts("\n");

	int my_port = (rand() % 10000)+40000;
	char port_s[5];
	sprintf(port_s, "%i", my_port);

	char* peer_id_encoded = url_enc(peer_id);
	//printf("URL encoded id outside: %s\n", peer_id_encoded);

	uint8_t *final1 = (uint8_t*) malloc(16);
	uint8_t *final2 = (uint8_t*) malloc(45);
	//48 54 54 50 2f 				31 2e 30 0d 0a
	final1[0] = 0x48; final1[1] = 0x54; final1[2] = 0x54; final1[3] = 0x50; final1[4] = 0x2f;
	final1[5] = 0x31; final1[6] = 0x2e; final1[7] = 0x30; final1[8] = 0x0d; final1[9] = 0x0a;
	//48 6f 73 74 3a 				20  62 74 74 72
	final1[10] = 0x48; final1[11] = 0x6f; final1[12] = 0x73; final1[13] = 0x74; final1[14] = 0x3a;
	final1[15] = 0x20;

	final2[0] = 0x0d; final2[1] = 0x0a; final2[2] = 0x55; final2[3] = 0x73;
	//65 72 2d 41 67 				65 6e 74  3a 20
	final2[4] = 0x65; final2[5] = 0x72; final2[6] = 0x2d; final2[7] = 0x41; final2[8] = 0x67;
	final2[9] = 0x65; final2[10] = 0x6e; final2[11] = 0x74; final2[12] = 0x3a; final2[13] = 0x20;
	//45 6e 68 61 6e 					63 65 64 2d 43
	final2[14] = 0x45; final2[15] = 0x6e; final2[16] = 0x68; final2[17] = 0x61; final2[18] = 0x6e;
	final2[19] = 0x63; final2[20] = 0x65; final2[21] = 0x64; final2[22] = 0x2d; final2[23] = 0x43;
	//54 6f 72 72  65 				6e 74 2f 64 6e
	final2[24] = 0x54; final2[25] = 0x6f; final2[26] = 0x72; final2[27] = 0x72; final2[28] = 0x65;
	final2[29] = 0x6e; final2[30] = 0x74; final2[31] = 0x2f; final2[32] = 0x64; final2[33] = 0x6e;
	//68 33 2e 33 2e 					32 0d 0a 0d 0a
	final2[34] = 0x68; final2[35] = 0x33; final2[36] = 0x2e; final2[37] = 0x33; final2[38] = 0x2e;
	final2[39] = 0x32; final2[40] = 0x0d; final2[41] = 0x0a; final2[42] = 0x0d; final2[43] = 0x0a;
	final2[44] = '\0';


	char buffer[strlen(info_hash_encoded) + strlen(peer_id_encoded) + strlen(port_s) + 110  +strlen(url)];
	sprintf(buffer, "%s.php?info_hash=%s&peer_id=%s&port=%s&compact=1 %s%s%s\n", http_get, info_hash_encoded, peer_id_encoded, port_s, final1, url, final2);

	// if(http_get != NULL){
	// 	free(http_get);
	// 	http_get = NULL;
	// }

	int sock;
    struct sockaddr_in server_addr;

    if((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		printf("Socket failed\n");
		exit(0);
	}

	//Hay que hacer un metodo que me corte bttracker.debian.org
	struct in_addr tracker_ip;




	tracker_ip = getTrackerIP(url);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(tracker_ip));
	server_addr.sin_port = htons(port);

	if(connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0){
		printf("Connect() failed");
		exit(0);
	}

	printf("Connected successfully\n");

	char recv_Buffer[2000];
	//int seg_fault = 0;
	int bytes_rcv;

	//while(seg_fault == 0){

		if(send(sock, buffer, strlen(buffer)-1, 0) != strlen(buffer)-1){
			printf("Send failed");
			exit(0);
		}




		if ((bytes_rcv = recv(sock, recv_Buffer, 2000, 0)) <= 0){
			printf("Receiving failed\n");
			exit(0);
		}

	// 	seg_fault = 1;
	// 	for(i = 0; i < strlen(recv_Buffer)-7; i++){
	// 		if(recv_Buffer[i] == 'f' && recv_Buffer[i+1] == 'a' &&  recv_Buffer[i+2] == 'i' &&
	// 			recv_Buffer[i+3] == 'l' && recv_Buffer[i+4] == 'u' &&  recv_Buffer[i+5] == 'r' &&
	// 			recv_Buffer[i+6] == 'e'){
	// 			seg_fault = 0;
	// 			i = strlen(recv_Buffer);
	// 		}
	// }


	//}

	//Get the information from the received buffer

	for(i = 0; i < strlen(recv_Buffer)-1; i++){
		if(recv_Buffer[i] == 0x0d && recv_Buffer[i+1] == 0x0a &&  recv_Buffer[i+2] == 0x0d && recv_Buffer[i+3] == 0x0a){
			found = i+4;
		}
	}

	char *init = (char*)malloc(bytes_rcv-found);

	char* pointer = NULL;
	pointer = &recv_Buffer[found];
	memcpy(init, pointer, bytes_rcv-found);

	//size_t aux = bytes_rcv-found;

	// node = be_decode(init, aux, &rx);
 //    BE_ASSERT(node != NULL);



     //be_node_t *info2 = be_dict_lookup(node, "peers", NULL);


    //Calculo el numero de peers con un length que hay que mirar en wireshark
    //Calculo distancia al diccionario desde d8...
    //realloc de init
    //decode del init
    //lookup

    final_num_peers = 0;

    for(i = 0; i < strlen(recv_Buffer)-5; i++){
		if(recv_Buffer[i] == 'p' && recv_Buffer[i+1] == 'e' &&  recv_Buffer[i+2] == 'e' && recv_Buffer[i+3] == 'r' && recv_Buffer[i+4] == 's'){
			found = i+6;

		}
	}

	init = (char*)realloc(init, bytes_rcv-found-2);
	pointer = NULL;
	pointer = &recv_Buffer[found];
	memcpy(init, pointer, bytes_rcv-found-2);


    node = be_decode(init, bytes_rcv-found-2, &rx);
    be_node_t *ip_dict = be_dict_lookup(node, "ip", NULL);

    be_node_t *port_dict = be_dict_lookup(node, "port", NULL);

    found = 0;


    for(i = 0; i < strlen(init); i++){
		if(init[i] == 'd' && init[i+1] == '2' && init[i+2] == ':' && init[i+3] == 'i' && init[i+4] == 'p'){
			if(found == 0)
				found = i;
			//printf("%c, %i\n\n\n", init[i], i);
			final_num_peers++;
		}
	}



	peer_arr = (struct peer*) realloc(peer_arr, sizeof(struct peer)*final_num_peers);

	peer_arr[0].ip = (char*)malloc(ip_dict->x.str.len);
	memcpy(peer_arr[0].ip, ip_dict->x.str.buf, ip_dict->x.str.len);
	peer_arr[0].port = port_dict->x.num;
	peer_arr[0].idx = 0;


    for(i = 0; i<final_num_peers-1; i++){


    	init = (char*)realloc(init, bytes_rcv-found);


		pointer = NULL;
		pointer = &init[found];


		memcpy(init, pointer, bytes_rcv-found-1);

	    node = be_decode(init, bytes_rcv-found-1, &rx);

	    ip_dict = be_dict_lookup(node, "ip", NULL);


	    port_dict = be_dict_lookup(node, "port", NULL);

	    peer_arr[i+1].ip = (char*)malloc(ip_dict->x.str.len);
		memcpy(peer_arr[i+1].ip, ip_dict->x.str.buf, ip_dict->x.str.len);
		peer_arr[i+1].port = port_dict->x.num;
		peer_arr[i+1].idx = i+1;


	    if(i != final_num_peers-2){
	    	for(j = 2; j < strlen(init)-1; j++){
				if(init[j] == 'd' && init[j+1] == '2' && init[j+2] == ':' && init[j+3] == 'i' && init[j+4] == 'p'){
					found = j;
					break;
				}

			}
	    }



    }


    //Compact

    // int num_peers = info2->x.str.len/6;
    // peer_arr = (struct peer*) malloc(sizeof(struct peer)*num_peers);
    // int it = 0, boole = 0, final_num_peers = 0;
    // i = 0;
    // unsigned int ip_aux, aux_length = 0;
    // char ip_aux_buf[3], ip_aux_buf2 [15];
    // for(i = 0; i<num_peers; i++){
    // 	peer_arr[i].ip = NULL;
    // 	for(it = 0; it<4; it++){
	   //  	ip_aux = (unsigned int)info2->x.str.buf[6*i+it] & 0xff;
	   //  	sprintf(ip_aux_buf, "%i", ip_aux);
	   //  	aux_length+=strlen(ip_aux_buf);
	   //  	if(it!=3) aux_length += 1;

	   //  	strcat(ip_aux_buf2, ip_aux_buf);
	   //  	if(it!=3) strcat(ip_aux_buf2, ".");

	 //    }
	 //    ip_aux_buf2[aux_length] = '\0';
	 //    for(j = 0; j<final_num_peers; j++){
	 //    	if(strcmp(ip_aux_buf2, peer_arr[j].ip) == 0){
	 //    		boole = 1;
	 //    		break;
	 //    	}
	 //    }
	 //    if(boole == 0){

		//     peer_arr[final_num_peers].ip = (char*) malloc(aux_length);
		//     memcpy(peer_arr[final_num_peers].ip, ip_aux_buf2, aux_length);
		//    	peer_arr[final_num_peers].ip[aux_length] = '\0';
		//     printf("Peer num %i: %s\n", final_num_peers, peer_arr[final_num_peers].ip);
		//     peer_arr[final_num_peers].idx = final_num_peers;

		//     //port
		//     peer_arr[final_num_peers].port = 256*((unsigned int)info2->x.str.buf[6*i+it] & 0xff) + ((unsigned int)info2->x.str.buf[6*i+it+1] & 0xff);
		// 	final_num_peers++;
		// }
		// aux_length = 0;
		// ip_aux_buf2[0] = '\0';
		// boole = 0;
  //   }


    choke_array = (int*) calloc(final_num_peers, sizeof(int));


	//HANDSHAKE MESSAGE
	 // 1 for protocol, 19 for protocol name(Bittorrent protocol),
												 // 8 reserved (00), 20 for info_hash and 20 for peer_id
	handshake[0] = 0x13; handshake[1] = 0x42; handshake[2] = 0x69; handshake[3] = 0x74; handshake[4] = 0x54;
	handshake[5] = 0x6f; handshake[6] = 0x72; handshake[7] = 0x72; handshake[8] = 0x65; handshake[9] = 0x6e;
	handshake[10] = 0x74; handshake[11] = 0x20; handshake[12] = 0x70; handshake[13] = 0x72; handshake[14] = 0x6f;
	handshake[15] = 0x74; handshake[16] = 0x6f; handshake[17] = 0x63; handshake[18] = 0x6f; handshake[19] = 0x6c;

	for(i = 0; i < 8; i++){
		handshake[20+i] = 0x00;
	}

	for(i = 0; i < 20; i++){
		handshake[28+i] = checksum[i];
	}
	for(i = 0; i < 20; i++){
		handshake[48+i] = peer_id[i];
	}

	thread_array = (pthread_t*)malloc(sizeof(pthread_t)*final_num_peers);

	while(downloaded == 0){
		for(i = 0; i<final_num_peers; i++){
		    pthread_create(&thread_array[i], NULL, function, &peer_arr[i]);
		}

		for(i = 0; i<final_num_peers; i++){
			pthread_join(thread_array[i], NULL);

		}

	}

	puts("FILE DOWNLOADED!!\n");

	int k;

	FILE *fp;
	fp = fopen(name_for_file->x.str.buf, "w");

	for(k = 0; k<num_pieces-1; k++){
		for(i = 0; i<num_subPieces; i++){
			for(j = 0; j<16384; j++){
				piece_to_hash[16384*i + j] = piecefield[k][i][j];
			}
		}
		//piece_to_hash(strlen(piece_to_hash)) = '\0';
		fwrite(piece_to_hash, sizeof(char), 16384*num_subPieces, fp);
	}
	int offs = last_size/16384;
	for(i = 0; i<offs; i++){
		for(j = 0; j<16384; j++){
			piece_to_hash[16384*i + j] = piecefield[k][i][j];
		}
	}
	for(j = 0; j<(last_size-(16384*offs)); j++){
		piece_to_hash[16384*i + j] = piecefield[k][i][j];
	}
	fwrite(piece_to_hash, sizeof(char), last_size, fp);
}
