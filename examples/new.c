/*
 * Demo on how to use /dev/crypto device for HMAC.
 *
 * Placed under public domain.
 *
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../ncr.h"
#include <stdlib.h>

#define DATA_SIZE 4096

static void randomize_data(uint8_t * data, size_t data_size)
{
int i;
	
	srand(time(0)*getpid());
	for (i=0;i<data_size;i++) {
		data[i] = rand() & 0xff;
	}
}

#define KEY_DATA_SIZE 16
#define WRAPPED_KEY_DATA_SIZE 32
static int
test_ncr_key(int cfd)
{
	struct ncr_data_init_st dinit;
	struct ncr_key_generate_st kgen;
	ncr_key_t key;
	struct ncr_key_data_st keydata;
	struct ncr_data_st kdata;
	uint8_t data[KEY_DATA_SIZE];
	uint8_t data_bak[KEY_DATA_SIZE];

	fprintf(stdout, "Tests on Keys:\n");

	/* test 1: generate a key in userspace import it
	 * to kernel via data and export it.
	 */

	fprintf(stdout, "\tKey generation...\n");

	randomize_data(data, sizeof(data));
	memcpy(data_bak, data, sizeof(data));

	dinit.max_object_size = KEY_DATA_SIZE;
	dinit.flags = NCR_DATA_FLAG_EXPORTABLE;
	dinit.initial_data = data;
	dinit.initial_data_size = sizeof(data);

	if (ioctl(cfd, NCRIO_DATA_INIT, &dinit)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	/* convert it to key */
	if (ioctl(cfd, NCRIO_KEY_INIT, &key)) {
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	keydata.key_id[0] = 'a';
	keydata.key_id[2] = 'b';
	keydata.key_id_size = 2;
	keydata.type = NCR_KEY_TYPE_SECRET;
	keydata.algorithm = NCR_ALG_AES_CBC;
	keydata.flags = NCR_KEY_FLAG_EXPORTABLE;
	
	keydata.key = key;
	keydata.data = dinit.desc;

	if (ioctl(cfd, NCRIO_KEY_IMPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	/* now try to read it */
	fprintf(stdout, "\tKey export...\n");
	if (ioctl(cfd, NCRIO_DATA_DEINIT, &dinit.desc)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_DEINIT)");
		return 1;
	}

	dinit.max_object_size = DATA_SIZE;
	dinit.flags = NCR_DATA_FLAG_EXPORTABLE;
	dinit.initial_data = NULL;
	dinit.initial_data_size = 0;

	if (ioctl(cfd, NCRIO_DATA_INIT, &dinit)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	memset(&keydata, 0, sizeof(keydata));
	keydata.key = key;
	keydata.data = dinit.desc;

	if (ioctl(cfd, NCRIO_KEY_EXPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	/* now read data */
	memset(data, 0, sizeof(data));

	kdata.desc = dinit.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_GET)");
		return 1;
	}

	if (memcmp(data, data_bak, sizeof(data))!=0) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		fprintf(stderr, "data returned but differ!\n");
		return 1;
	}

	if (ioctl(cfd, NCRIO_KEY_DEINIT, &key)) {
		perror("ioctl(NCRIO_KEY_DEINIT)");
		return 1;
	}

	/* finished, we keep data for next test */

	/* test 2: generate a key in kernel space and
	 * export it.
	 */

	fprintf(stdout, "\tKey import...\n");
	/* convert it to key */
	if (ioctl(cfd, NCRIO_KEY_INIT, &key)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	kgen.desc = key;
	kgen.params.algorithm = NCR_ALG_AES_CBC;
	kgen.params.keyflags = NCR_KEY_FLAG_EXPORTABLE;
	kgen.params.params.secret.bits = 128; /* 16  bytes */
	
	if (ioctl(cfd, NCRIO_KEY_GENERATE, &kgen)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	memset(&keydata, 0, sizeof(keydata));
	keydata.key = key;
	keydata.data = dinit.desc;

	if (ioctl(cfd, NCRIO_KEY_EXPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	/* now read data */
	memset(data, 0, sizeof(data));

	kdata.desc = dinit.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_GET)");
		return 1;
	}

#if 0
	fprintf(stderr, "Generated key: %.2x.%.2x.%.2x.%.2x.%.2x.%.2x.%.2x.%.2x."
		"%.2x.%.2x.%.2x.%.2x.%.2x.%.2x.%.2x.%.2x\n", data[0], data[1],
		data[2], data[3], data[4], data[5], data[6], data[7], data[8],
		data[9], data[10], data[11], data[12], data[13], data[14],
		data[15]);
#endif

	if (ioctl(cfd, NCRIO_KEY_DEINIT, &key)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_DEINIT)");
		return 1;
	}
	
	/* test 3: generate an unexportable key in kernel space and
	 * try to export it.
	 */
	fprintf(stdout, "\tKey protection of non-exportable keys...\n");
	if (ioctl(cfd, NCRIO_KEY_INIT, &key)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	kgen.desc = key;
	kgen.params.algorithm = NCR_ALG_AES_CBC;
	kgen.params.keyflags = 0;
	kgen.params.params.secret.bits = 128; /* 16  bytes */
	
	if (ioctl(cfd, NCRIO_KEY_GENERATE, &kgen)) {
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	memset(&keydata, 0, sizeof(keydata));
	keydata.key = key;
	keydata.data = dinit.desc;

	if (ioctl(cfd, NCRIO_KEY_EXPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_EXPORT)");
		return 1;
	}

	/* try to get the output data - should fail */
	memset(data, 0, sizeof(data));

	kdata.desc = dinit.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)==0) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		fprintf(stderr, "Data were exported, but shouldn't be!\n");
		return 1;
	}

	if (ioctl(cfd, NCRIO_KEY_DEINIT, &key)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_DEINIT)");
		return 1;
	}

	return 0;
}


static int test_ncr_data(int cfd)
{
	struct ncr_data_init_st init;
	struct ncr_data_st kdata;
	uint8_t data[DATA_SIZE];
	uint8_t data_bak[DATA_SIZE];
	int i;

	fprintf(stdout, "Tests on Data:\n");
	
	randomize_data(data, sizeof(data));
	memcpy(data_bak, data, sizeof(data));

	init.max_object_size = DATA_SIZE;
	init.flags = NCR_DATA_FLAG_EXPORTABLE;
	init.initial_data = data;
	init.initial_data_size = sizeof(data);

	if (ioctl(cfd, NCRIO_DATA_INIT, &init)) {
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}
	
	fprintf(stdout, "\tData Import...\n");

	memset(data, 0, sizeof(data));

	kdata.desc = init.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
		perror("ioctl(NCRIO_DATA_GET)");
		return 1;
	}

	if (memcmp(data, data_bak, sizeof(data))!=0) {
		fprintf(stderr, "data returned but differ!\n");
		return 1;
	}

	fprintf(stdout, "\tData Export...\n");

	/* test set */
	memset(data, 0xf1, sizeof(data));

	kdata.desc = init.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_SET, &kdata)) {
		perror("ioctl(NCRIO_DATA_SET)");
		return 1;
	}

	/* test get after set */
	memset(data, 0, sizeof(data));

	kdata.desc = init.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
		perror("ioctl(NCRIO_DATA_GET)");
		return 1;
	}

	for(i=0;i<kdata.data_size;i++) {
		if (((uint8_t*)kdata.data)[i] != 0xf1) {
			fprintf(stderr, "data returned but differ!\n");
			return 1;
		}
	}
	fprintf(stdout, "\t2nd Data Import/Export...\n");

	if (ioctl(cfd, NCRIO_DATA_DEINIT, &kdata.desc)) {
		perror("ioctl(NCRIO_DATA_DEINIT)");
		return 1;
	}

	fprintf(stdout, "\tProtection of non-exportable data...\n");
	randomize_data(data, sizeof(data));

	init.max_object_size = DATA_SIZE;
	init.flags = 0;
	init.initial_data = data;
	init.initial_data_size = sizeof(data);

	if (ioctl(cfd, NCRIO_DATA_INIT, &init)) {
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	kdata.desc = init.desc;
	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)==0) {
		fprintf(stderr, "Unexportable data were exported!?\n");
		return 1;
	}

	fprintf(stdout, "\tLimits on maximum allowed data...\n");
	for (i=0;i<256;i++ ) {
		init.max_object_size = DATA_SIZE;
		init.flags = 0;
		init.initial_data = data;
		init.initial_data_size = sizeof(data);

		if (ioctl(cfd, NCRIO_DATA_INIT, &init)) {
			//fprintf(stderr, "Reached maximum limit at: %d data items\n", i);
			break;
		}
	}
	
	/* shouldn't run any other tests after that */

	return 0;
}

/* Key wrapping */
static int
test_ncr_wrap_key(int cfd)
{
	int i;
	struct ncr_data_init_st dinit;
	struct ncr_key_generate_st kgen;
	ncr_key_t key, key2;
	struct ncr_key_data_st keydata;
	struct ncr_data_st kdata;
	struct ncr_key_wrap_st kwrap;
	uint8_t data[WRAPPED_KEY_DATA_SIZE];


	fprintf(stdout, "Tests on Keys:\n");

	/* test 1: generate a key in userspace import it
	 * to kernel via data and export it.
	 */

	fprintf(stdout, "\tKey Wrap test...\n");

	dinit.max_object_size = WRAPPED_KEY_DATA_SIZE;
	dinit.flags = NCR_DATA_FLAG_EXPORTABLE;
	dinit.initial_data = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F";
	dinit.initial_data_size = 16;

	if (ioctl(cfd, NCRIO_DATA_INIT, &dinit)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	/* convert it to key */
	if (ioctl(cfd, NCRIO_KEY_INIT, &key)) {
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	keydata.key_id[0] = 'a';
	keydata.key_id[2] = 'b';
	keydata.key_id_size = 2;
	keydata.type = NCR_KEY_TYPE_SECRET;
	keydata.algorithm = NCR_ALG_AES_CBC;
	keydata.flags = NCR_KEY_FLAG_EXPORTABLE|NCR_KEY_FLAG_WRAPPABLE;
	
	keydata.key = key;
	keydata.data = dinit.desc;

	if (ioctl(cfd, NCRIO_KEY_IMPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

#define DKEY "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF"
	/* now key data */
	kdata.data = DKEY;
	kdata.data_size = 16;
	kdata.desc = dinit.desc;
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_SET, &kdata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	/* convert it to key */
	if (ioctl(cfd, NCRIO_KEY_INIT, &key2)) {
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	keydata.key_id[0] = 'b';
	keydata.key_id[2] = 'a';
	keydata.key_id_size = 2;
	keydata.type = NCR_KEY_TYPE_SECRET;
	keydata.algorithm = NCR_ALG_AES_CBC;
	keydata.flags = NCR_KEY_FLAG_EXPORTABLE|NCR_KEY_FLAG_WRAPPABLE;
	
	keydata.key = key2;
	keydata.data = kdata.desc;

	if (ioctl(cfd, NCRIO_KEY_IMPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	/* now try wrapping key2 using key */
	memset(&kwrap, 0, sizeof(kwrap));
	kwrap.algorithm = NCR_WALG_AES_RFC3394;
	kwrap.keytowrap = key2;
	kwrap.key.key = key;
	kwrap.data = kdata.desc;

	if (ioctl(cfd, NCRIO_KEY_WRAP, &kwrap)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_WRAP)");
		return 1;
	}

	kdata.data = data;
	kdata.data_size = sizeof(data);
	kdata.append_flag = 0;

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_GET)");
		return 1;
	}

	if (kdata.data_size != 24 || memcmp(kdata.data,
		"\x1F\xA6\x8B\x0A\x81\x12\xB4\x47\xAE\xF3\x4B\xD8\xFB\x5A\x7B\x82\x9D\x3E\x86\x23\x71\xD2\xCF\xE5", 24) != 0) {
		fprintf(stderr, "Wrapped data do not match.\n");

		fprintf(stderr, "Data[%d]: ",(int) kdata.data_size);
		for(i=0;i<kdata.data_size;i++)
			fprintf(stderr, "%.2x:", data[i]);
		fprintf(stderr, "\n");
		return 1;
	}




	/* test unwrapping */
	fprintf(stdout, "\tKey Unwrap test...\n");

	/* reset key2 */
	if (ioctl(cfd, NCRIO_KEY_DEINIT, &key2)) {
		perror("ioctl(NCRIO_KEY_DEINIT)");
		return 1;
	}

	if (ioctl(cfd, NCRIO_KEY_INIT, &key2)) {
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	memset(&kwrap, 0, sizeof(kwrap));
	kwrap.algorithm = NCR_WALG_AES_RFC3394;
	kwrap.keytowrap = key2;
	kwrap.key.key = key;
	kwrap.data = kdata.desc;

	if (ioctl(cfd, NCRIO_KEY_UNWRAP, &kwrap)) {
		perror("ioctl(NCRIO_KEY_UNWRAP)");
		return 1;
	}

	/* now export the unwrapped */
#if 0
	/* this cannot be performed like that, because unwrap
	 * always sets keys as unexportable. Maybe we can implement
	 * a data comparison ioctl().
	 */
	memset(&keydata, 0, sizeof(keydata));
	keydata.key = key2;
	keydata.data = kdata.desc;

	if (ioctl(cfd, NCRIO_KEY_EXPORT, &keydata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_KEY_IMPORT)");
		return 1;
	}

	if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_GET)");
		return 1;
	}

	if (kdata.data_size != 16 || memcmp(kdata.data, DKEY, 16) != 0) {
		fprintf(stderr, "Unwrapped data do not match.\n");
		fprintf(stderr, "Data[%d]: ", (int) kdata.data_size);
		for(i=0;i<kdata.data_size;i++)
			fprintf(stderr, "%.2x:", data[i]);
		fprintf(stderr, "\n");
		return 1;
	}
#endif



}

struct aes_vectors_st {
	const uint8_t* key;
	const uint8_t* plaintext;
	const uint8_t* ciphertext;
} aes_vectors[] = {
	{
		.key = "\xc0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
		.plaintext = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
		.ciphertext = "\x4b\xc3\xf8\x83\x45\x0c\x11\x3c\x64\xca\x42\xe1\x11\x2a\x9e\x87",
	},
	{
		.key = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
		.plaintext = "\xf3\x44\x81\xec\x3c\xc6\x27\xba\xcd\x5d\xc3\xfb\x08\xf2\x73\xe6",
		.ciphertext = "\x03\x36\x76\x3e\x96\x6d\x92\x59\x5a\x56\x7c\xc9\xce\x53\x7f\x5e",
	},
	{
		.key = "\x10\xa5\x88\x69\xd7\x4b\xe5\xa3\x74\xcf\x86\x7c\xfb\x47\x38\x59",
		.plaintext = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
		.ciphertext = "\x6d\x25\x1e\x69\x44\xb0\x51\xe0\x4e\xaa\x6f\xb4\xdb\xf7\x84\x65",
	},
	{
		.key = "\xca\xea\x65\xcd\xbb\x75\xe9\x16\x9e\xcd\x22\xeb\xe6\xe5\x46\x75",
		.plaintext = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
		.ciphertext = "\x6e\x29\x20\x11\x90\x15\x2d\xf4\xee\x05\x81\x39\xde\xf6\x10\xbb",
	},
	{
		.key = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfe",
		.plaintext = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
		.ciphertext = "\x9b\xa4\xa9\x14\x3f\x4e\x5d\x40\x48\x52\x1c\x4f\x88\x77\xd8\x8e",
	},
};

/* Key wrapping */
static int
test_ncr_aes(int cfd)
{
	struct ncr_data_init_st dinit;
	struct ncr_key_generate_st kgen;
	ncr_key_t key;
	struct ncr_key_data_st keydata;
	struct ncr_data_st kdata;
	ncr_data_t dd, dd2;
	uint8_t data[KEY_DATA_SIZE];
	int i, j;
	struct ncr_session_once_op_st nop;

	dinit.max_object_size = KEY_DATA_SIZE;
	dinit.flags = NCR_DATA_FLAG_EXPORTABLE;
	dinit.initial_data = NULL;
	dinit.initial_data_size = 0;

	if (ioctl(cfd, NCRIO_DATA_INIT, &dinit)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	dd = dinit.desc;

	if (ioctl(cfd, NCRIO_DATA_INIT, &dinit)) {
		fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
		perror("ioctl(NCRIO_DATA_INIT)");
		return 1;
	}

	dd2 = dinit.desc;

	/* convert it to key */
	if (ioctl(cfd, NCRIO_KEY_INIT, &key)) {
		perror("ioctl(NCRIO_KEY_INIT)");
		return 1;
	}

	keydata.key_id[0] = 'a';
	keydata.key_id[2] = 'b';
	keydata.key_id_size = 2;
	keydata.type = NCR_KEY_TYPE_SECRET;
	keydata.algorithm = NCR_ALG_AES_CBC;
	keydata.flags = NCR_KEY_FLAG_EXPORTABLE;
	

	fprintf(stdout, "Tests on AES Encryption\n");
	for (i=0;i<sizeof(aes_vectors)/sizeof(aes_vectors[0]);i++) {

		/* import key */
		kdata.data = (void*)aes_vectors[i].key;
		kdata.data_size = 16;
		kdata.desc = dd;
		kdata.append_flag = 0;

		if (ioctl(cfd, NCRIO_DATA_SET, &kdata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_DATA_INIT)");
			return 1;
		}

		keydata.key = key;
		keydata.data = dd;
		if (ioctl(cfd, NCRIO_KEY_IMPORT, &keydata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_KEY_IMPORT)");
			return 1;
		}
		/* import data */

		kdata.data = (void*)aes_vectors[i].plaintext;
		kdata.data_size = 16;
		kdata.desc = dd;
		kdata.append_flag = 0;

		if (ioctl(cfd, NCRIO_DATA_SET, &kdata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_DATA_INIT)");
			return 1;
		}

		/* encrypt */
		memset(&nop, 0, sizeof(nop));
		nop.init.algorithm = NCR_ALG_AES_ECB;
		nop.init.params.key = key;
		nop.init.op = NCR_OP_ENCRYPT;
		nop.op.data.cipher.plaintext = dd;
		nop.op.data.cipher.ciphertext = dd2;

		if (ioctl(cfd, NCRIO_SESSION_ONCE, &nop)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_SESSION_ONCE)");
			return 1;
		}

		/* verify */
		kdata.desc = dd2;
		kdata.data = data;
		kdata.data_size = sizeof(data);
		kdata.append_flag = 0;

		if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_DATA_GET)");
			return 1;
		}

		if (kdata.data_size != 16 || memcmp(kdata.data, aes_vectors[i].ciphertext, 16) != 0) {
			fprintf(stderr, "AES test vector %d failed!\n", i);

			fprintf(stderr, "Cipher[%d]: ", (int)kdata.data_size);
			for(j=0;j<kdata.data_size;j++)
			  fprintf(stderr, "%.2x:", (int)data[j]);
			fprintf(stderr, "\n");

			fprintf(stderr, "Expected[%d]: ", 16);
			for(j=0;j<16;j++)
			  fprintf(stderr, "%.2x:", (int)aes_vectors[i].ciphertext[j]);
			fprintf(stderr, "\n");
			return 1;
		}
	}

	fprintf(stdout, "Tests on AES Decryption\n");
	for (i=0;i<sizeof(aes_vectors)/sizeof(aes_vectors[0]);i++) {

		/* import key */
		kdata.data = (void*)aes_vectors[i].key;
		kdata.data_size = 16;
		kdata.desc = dd;
		kdata.append_flag = 0;

		if (ioctl(cfd, NCRIO_DATA_SET, &kdata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_DATA_INIT)");
			return 1;
		}

		keydata.key = key;
		keydata.data = dd;
		if (ioctl(cfd, NCRIO_KEY_IMPORT, &keydata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_KEY_IMPORT)");
			return 1;
		}

		/* import ciphertext */

		kdata.data = (void*)aes_vectors[i].ciphertext;
		kdata.data_size = 16;
		kdata.desc = dd;
		kdata.append_flag = 0;

		if (ioctl(cfd, NCRIO_DATA_SET, &kdata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_DATA_INIT)");
			return 1;
		}

		/* decrypt */
		memset(&nop, 0, sizeof(nop));
		nop.init.algorithm = NCR_ALG_AES_ECB;
		nop.init.params.key = key;
		nop.init.op = NCR_OP_DECRYPT;
		nop.op.data.cipher.ciphertext = dd;
		nop.op.data.cipher.plaintext = dd2;

		if (ioctl(cfd, NCRIO_SESSION_ONCE, &nop)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_SESSION_ONCE)");
			return 1;
		}

		/* verify */
		kdata.desc = dd2;
		kdata.data = data;
		kdata.data_size = sizeof(data);
		kdata.append_flag = 0;

		if (ioctl(cfd, NCRIO_DATA_GET, &kdata)) {
			fprintf(stderr, "Error: %s:%d\n", __func__, __LINE__);
			perror("ioctl(NCRIO_DATA_GET)");
			return 1;
		}

		if (kdata.data_size != 16 || memcmp(kdata.data, aes_vectors[i].plaintext, 16) != 0) {
			fprintf(stderr, "AES test vector %d failed!\n", i);

			fprintf(stderr, "Plain[%d]: ", (int)kdata.data_size);
			for(j=0;j<kdata.data_size;j++)
			  fprintf(stderr, "%.2x:", (int)data[j]);
			fprintf(stderr, "\n");

			fprintf(stderr, "Expected[%d]: ", 16);
			for(j=0;j<16;j++)
			  fprintf(stderr, "%.2x:", (int)aes_vectors[i].plaintext[j]);
			fprintf(stderr, "\n");
//			return 1;
		}
	}


	fprintf(stdout, "\n");

	return 0;

}

int
main()
{
	int fd = -1;

	/* Open the crypto device */
	fd = open("/dev/crypto", O_RDWR, 0);
	if (fd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	/* Run the test itself */
	if (test_ncr_data(fd))
		return 1;

	/* Close the original descriptor */
	if (close(fd)) {
		perror("close(fd)");
		return 1;
	}

	/* actually test if the initial close
	 * will really delete all used lists */

	fd = open("/dev/crypto", O_RDWR, 0);
	if (fd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}
	if (test_ncr_key(fd))
		return 1;

	if (test_ncr_aes(fd))
		return 1;

	if (test_ncr_wrap_key(fd))
		return 1;

	/* Close the original descriptor */
	if (close(fd)) {
		perror("close(fd)");
		return 1;
	}

	return 0;
}
