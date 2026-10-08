#include <cli.h>
#include <command.h>
#include <env.h>
#include <linux/errno.h>
#include <net.h>
#include <vsprintf.h>

#define GOLDEN_IMAGE_OFFSET 0x8000000
#define _0_PATTERN_OFFSET 0x180000 /* start addr of factory partition */
#define _A5_PATTERN_OFFSET 0x280000
#define _5A_PATTERN_OFFSET 0x380000
#define _1_PATTERN_OFFSET 0x480000

static int do_nand_stress(struct cmd_tbl *cmdtp, int flag, int argc,
			  char * const argv[])
{
	ulong data_load_addr = CONFIG_SYS_LOAD_ADDR;
	u32 size = 0x1420000;
	char cmd[256];
	int i = 1;

	snprintf(cmd, sizeof(cmd),
		 "nmbm nmbm0 read 0x%lx 0x0 0x%x", data_load_addr, size);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "nmbm nmbm0 read 0x%lx 0x0 0x%x",
		 data_load_addr + GOLDEN_IMAGE_OFFSET, size);
	run_command(cmd, 0);

	/* Since load address is 0x48000000 now,
	 * between 0x48180000-0x48580000, insert the following test pattern:
	 * 0x48180000-0x4827ffff: All zero
	 * 0x48280000-0x4829ffff: 0xa5a5....
	 * 0x48380000-0x4839ffff: 0x5a5a....
	 * 0x48480000-0x4849ffff: All one
	 */
	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0x0 0x100000",
		 data_load_addr + _0_PATTERN_OFFSET);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0xa5 0x100000",
		 data_load_addr + _A5_PATTERN_OFFSET);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0x5a 0x100000",
		 data_load_addr + _5A_PATTERN_OFFSET);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0xff 0x100000",
		 data_load_addr + _1_PATTERN_OFFSET);
	run_command(cmd, 0);


	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0x0 0x100000",
		 data_load_addr + GOLDEN_IMAGE_OFFSET + _0_PATTERN_OFFSET);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0xa5 0x100000",
		 data_load_addr + GOLDEN_IMAGE_OFFSET + _A5_PATTERN_OFFSET);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0x5a 0x100000",
		 data_load_addr + GOLDEN_IMAGE_OFFSET + _5A_PATTERN_OFFSET);
	run_command(cmd, 0);

	snprintf(cmd, sizeof(cmd),
		 "mw.b 0x%lx 0xff 0x100000",
		 data_load_addr + GOLDEN_IMAGE_OFFSET + _1_PATTERN_OFFSET);
	run_command(cmd, 0);


	printf("Starting NAND stress test...\n");
	while(1) {
		printf("[Round %d]\n", i);
		snprintf(cmd, sizeof(cmd),
			 "nmbm nmbm0 erase 0x0 0x%x", size);
		run_command(cmd, 0);

		snprintf(cmd, sizeof(cmd),
			 "nmbm nmbm0 write 0x%lx 0x0 0x%x",
			 data_load_addr, size);
		run_command(cmd, 0);

		snprintf(cmd, sizeof(cmd),
			 "nmbm nmbm0 read 0x%lx 0x0 0x%x",
			 data_load_addr + GOLDEN_IMAGE_OFFSET, size);
		run_command(cmd, 0);

		if (memcmp((void *)data_load_addr,
			   (void*)(data_load_addr + GOLDEN_IMAGE_OFFSET), size)) {
			printf("Data compare error\n");
			break;
		}

		run_command("nmbm nmbm0 state", 0);
		run_command("nmbm nmbm0 bad", 0);
		printf("====================================================\n");
		i++;
		if (!strncmp(argv[1], "-s", 2))
			break;
	}

	return 0;
}

U_BOOT_CMD(
	nand_stress, 2, 1, do_nand_stress,
	"perform NAND stress test and download a file via TFTP",
	"Usage: nand_stress\n"
);
