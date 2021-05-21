# Sentinel
Efficient-Tensor-Management-on-HM-for-Deep-Learning


Sentinel is an efficient tensor management solution on heterogeneous memory for DL workload with CPU. Sentinel profiles tensor access at page level, and migrate tensors based on the profiling result.

To use Sentinel, first you need to install profiler in linux-stable. The profiler in Sentinel is a tool to instrument page access. It converts page access to reserved page faults by using the reserved bit in a page table entry. The tool is build on linux kernel v4.9.

We implement tensor migration with Tensorflow v1.13. Specifially, Sentinel adds system calls to enable and disable profiling in Tensorflow runtime. Based on the profiling result, Sentinel migrates memory pages which contrains inactive tensors into slow memory and memory pages which contrains active tensors into fast memory.

To install profiler which implemented in linux kernel.
```
# copy current config as a base
cp /boot/config-$(uname -r) .config

make menuconfig

# compile all components
make -j && make -j modules && sudo make modules_install && sudo make install

sudo mkinitramfs -o /boot/initrd.img-4.9+
sudo update-initramfs -c -k 4.9+
sudo update-grub2

#verify:
ls /boot | grep "4.9+"
cat /boot/grub/grub.cfg|grep menuentry

# switch to 4.9+
# modify GRUB_DEFAULT=
# e.g. GRUB_DEFAULT="1"
# e.g. GRUB_DEFAULT="1>0"
# Please read https://help.ubuntu.com/community/Grub2/Submenus
vi /etc/default/grub

# update
sudo update-grub2
```

To install Tensorflow 
```
# Install bazel v0.21.0 (https://docs.bazel.build/versions/master/install.html)

# Clean tensorflow and install it takes tens of minutes
bazel clean --expunge
bazel build --config=opt //tensorflow/tools/pip_package:build_pip_package
bazel build -c opt --copt=-mavx --copt=-mavx2 --copt=-msse4.1 --copt=-msse4.2 -k //tensorflow/tools/pip_package:build_pip_package

./bazel-bin/tensorflow/tools/pip_package/build_pip_package /tmp/tensorflow_pkg

pip uninstall /tmp/tensorflow_pkg/tensorflow-
pip install --user /tmp/tensorflow_pkg/tensorflow-1.13.0rc2-cp35-cp35m-linux_x86_64.whl
```

Sentinel uses memnode 0 as fast memory and memnode 1 as slow memory. 

