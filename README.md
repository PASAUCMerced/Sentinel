# Sentinel
Efficient-Tensor-Management-on-HM-for-Deep-Learning

Sentinel is an efficient tensor management solution on heterogeneous memory for DL workload with CPU. Sentinel profiles tensor access at page level, and migrate tensors based on the profiling result.

To use Sentinel, first you need to install profiler in linux-stable. The profiler in Sentinel is a tool to instrument page access. It converts page access to reserved page faults by using the reserved bit in a page table entry. The tool is build on linux kernel v4.9.

We implement tensor migration with Tensorflow v1.13. Specifially, Sentinel adds system calls to enable and disable profiling in Tensorflow runtime. Based on the profiling result, Sentinel migrates memory pages which contrains inactive tensors into slow memory and memory pages which contrains active tensors into fast memory. 
