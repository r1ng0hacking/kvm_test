#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static int kvm,vmfd,vcpufd,ret;
static int code_fd;
static uint8_t *mem_slot0,*mem_slot1;
static struct kvm_sregs sregs;
static size_t mmap_size;
static struct kvm_run *run;

int main(void)
{
	
	code_fd = open("code",O_RDWR|O_CLOEXEC);
	if(code_fd == -1)
		err(1,"open code");
	
	//vm init
	kvm = open("/dev/kvm",O_RDWR|O_CLOEXEC);
	if(kvm == -1)
		err(1,"/dev/kvm");
	
	ret = ioctl(kvm,KVM_GET_API_VERSION,NULL);
	if(ret == -1)
		err(1,"KVM_GET_API_VERSION");
	if(ret != 12)
		errx(1,"KVM_GET_API_VERSION %d, expected 12",ret);

	vmfd = ioctl(kvm,KVM_CREATE_VM,(unsigned long)0);
	if(vmfd == -1)
		err(1,"KVM_CREATE_VM");
	
	//mm init
	mem_slot0 = mmap(NULL,0x1000,PROT_READ|PROT_WRITE,MAP_SHARED,code_fd,0);
	if(mem_slot0 == MAP_FAILED)
		err(1,"allocating guest memory");
	
	struct kvm_userspace_memory_region mem_region0 = {
		.slot = 0,
		.guest_phys_addr = 0x1000,
		.memory_size = 0x1000,
		.userspace_addr = (uint64_t)mem_slot0,
	};
	
	ret = ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&mem_region0);
	if(ret == -1)
		err(1,"KVM_SET_USER_MEMORY_REGION");
	
	mem_slot1 = mmap(NULL,0x1000,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANON,-1,0);
	if( mem_slot1 == MAP_FAILED)
		err(1,"alloctaing mem_slot1");
	mem_slot1[0] = 'r';
	mem_slot1[1] = 'l';
	struct kvm_userspace_memory_region mem_region1 = {
		.slot = 1,
		.guest_phys_addr = 0x2000,
		.memory_size = 0x1000,
		.userspace_addr = (uint64_t)mem_slot1,
	};
	
	ret = ioctl(vmfd,KVM_SET_USER_MEMORY_REGION,&mem_region1);
	if(ret == -1)
		err(1,"KVM_SET_USER_MEMORY_REGION");
	
	//cpu init
	vcpufd = ioctl(vmfd,KVM_CREATE_VCPU,(unsigned long)0);
	if(vcpufd == -1)
		err(1,"KVM_CREATE_VCPU");

	ret = ioctl(kvm,KVM_GET_VCPU_MMAP_SIZE,NULL);
	if(ret == -1)
		err(1,"KVM_GET_VCPU_MMAP_SIZE");
	mmap_size = ret;
	if(mmap_size < sizeof(*run))
		errx(1,"KVM_GET_VCPU_MMAP_SIZE unexpectedly samll");
	run = mmap(NULL,mmap_size,PROT_READ|PROT_WRITE,MAP_SHARED,vcpufd,0);
	if(!run)
		err(1,"mmap vcpu");
	
	ret = ioctl(vcpufd,KVM_GET_SREGS,&sregs);
	if(ret == -1)
		err(1,"KVM_GET_SREGS");
	sregs.cs.base = 0;
	sregs.cs.selector = 0;
	ret = ioctl(vcpufd,KVM_SET_SREGS,&sregs);
	if(ret == -1)
		err(1,"KVM_SET_SREGS");

	struct kvm_regs regs = {
		.rip = 0x1000,
		.rax = 2,
		.rbx = 2,
		.rflags = 0x2,
	};
	ret = ioctl(vcpufd,KVM_SET_REGS,&regs);
	if(ret == -1)
		err(1,"KVM_SET_REGS");

	while(1){
		ret = ioctl(vcpufd,KVM_RUN,NULL);
		if(ret == -1)
			err(1,"KVM_RUN");
		switch(run->exit_reason){
		case KVM_EXIT_HLT:
			puts("KVM_EXIT_HLT");
			return 0;
		case KVM_EXIT_IO:
			if(run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x3f8 && run->io.count == 1)
				putchar(*(((char *)run) + run->io.data_offset));
			else{
				if(run->io.direction == KVM_EXIT_IO_OUT) printf("KVM_EXIT_IO_OUT\n");
				if(run->io.size == 1) printf("1\n");
				if(run->io.port == 0x3f8) printf("0x3f8\n"); else printf("%d\n",run->io.port);
				errx(1,"unhandled KVM_EXIT_IO");
			}
			break;
		case KVM_EXIT_FAIL_ENTRY:
			 errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",(unsigned long long)run->fail_entry.hardware_entry_failure_reason);
			 break;
		case KVM_EXIT_INTERNAL_ERROR:
			 errx(1,"KVM_EXIT_INTERNAL_ERROR:suberror = 0x%x",run->internal.suberror);
			 break;
		default:
			 err(1,"exit_reason = 0x%x",run->exit_reason);
		}
	}

	return 0;
}
