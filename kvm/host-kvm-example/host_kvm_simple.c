#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define E_OK      (0)
#define E_NOT_OK  (1)

static int kvm_fd;
static int vm_fd;
static int vcpu_fd;
static void *mem;
static struct kvm_run *run;

void static cp_guest_img(int img_fd, void *mem)
{
  char *p = (char *)mem;
  for (;;) {
    int r = read(img_fd, p, 4096);
    if (r <= 0) {
      break;
    }
    p += r;
  }

}

static int vm_setup(int img_fd, int kvm_fd, int *vm_fd_p)
{
  struct kvm_userspace_memory_region region;
  int ret = E_OK;

  if ((*vm_fd_p = ioctl(kvm_fd, KVM_CREATE_VM, 0)) < 0) {
    perror("vm create failed\n");
    ret = E_NOT_OK;
    goto err;
  }

  if ((mem = mmap(NULL, 1 << 30, PROT_READ | PROT_WRITE,
		  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)) ==
      NULL) {
    perror("mmap failed\n");
    ret = E_NOT_OK;
    goto err;
  }

  memset(&region, 0, sizeof(region));
  region.slot = 0;
  region.guest_phys_addr = 0;
  region.memory_size = 1 << 30;
  region.userspace_addr = (uintptr_t)mem;

  if (ioctl(*vm_fd_p, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
    perror("ioctl user mem region failed");
    ret = E_NOT_OK;
    goto err;
  }
  cp_guest_img(img_fd, mem);

err:
  return ret;
}

static int vcpu_reg(int vcpu_fd)
{
  struct kvm_regs regs = {0}; // Clear all FLAGS bits
  struct kvm_sregs sregs = {0}; // real mode
  int ret = E_OK;

  regs.rflags = 2;
  regs.rip = 0;
  
  if (ioctl(vcpu_fd, KVM_GET_SREGS, &(sregs)) < 0) {
    perror("kvm get sregs failed\n");
    ret = E_NOT_OK;
    goto err;
  }

  if (ioctl(vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
    perror("kvm sregs failed");
    ret = E_NOT_OK;
    goto err;
  }

  if (ioctl(vcpu_fd, KVM_SET_REGS, &(regs)) < 0) {
    perror("kvm set regs failed\n");
    ret = E_NOT_OK;
    goto err;
  }

err:
  return ret;
}

static int vcpu_setup(int vm_fd, int kvm_fd,
  int *vcpu_fd_p, struct kvm_run **run)
{
  int ret = E_OK;
  
  if ((*vcpu_fd_p = ioctl(vm_fd, KVM_CREATE_VCPU, 0)) < 0) {
    perror("create vcpu failed");
    ret = E_NOT_OK;
    goto err;
  }
  int kvm_run_mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
  if (kvm_run_mmap_size < 0) {
    perror("vcpu mmap failed\n");
    ret = E_NOT_OK;
    goto err;
  }
  
  *run = (struct kvm_run *)mmap(
      NULL, kvm_run_mmap_size,
      PROT_READ | PROT_WRITE, MAP_SHARED, *vcpu_fd_p, 0);
  if (*run == NULL) {
    perror("mmap kvm_run\n");
    ret = E_NOT_OK;
    goto err;
  }
  ret = vcpu_reg(*vcpu_fd_p);

err:
  return ret;
}

static int run_and_emulate(int vcpu_fd, struct kvm_run *run)
{
  int ret = E_OK;
  for (;;) {
    int ret = ioctl(vcpu_fd, KVM_RUN, 0);
    if (ret < 0) {
      perror("KVM_RUN failed\n");
      ret = E_NOT_OK;
      goto err;
    }

    switch (run->exit_reason) {
    case KVM_EXIT_IO:
      char * p = (char *)run;
		  printf("Counter loop : %x\n", *(int *)(p + run->io.data_offset));
      sleep(1);
      break;
    case KVM_EXIT_SHUTDOWN:
      ret = E_NOT_OK;
      goto err;
    }
  }

err:
  return ret;
} 

int main(int argc, char *argv[])
{
  int ret = E_OK;
  
  if ((kvm_fd = open("/dev/kvm", O_RDWR)) < 0) {
    perror("open /dev/kvm failed");
    return E_NOT_OK;
  }

  int img_fd = open(argv[1], O_RDONLY);
  if (img_fd < 0) {
    perror("open guest image failed");
    ret = E_NOT_OK;
    goto close_kvm;
  }

  ret = vm_setup(img_fd, kvm_fd, &vm_fd);
  if (ret)
    goto close;
  
  ret = vcpu_setup(vm_fd, kvm_fd, &vcpu_fd, &run);
  if (ret)
    goto close;

  ret = run_and_emulate(vcpu_fd, run);

close:
  close(img_fd);
close_kvm:
  close(kvm_fd);
  
  return ret;
}




