# Sentinel
Efficient-Tensor-Management-on-HM-for-Deep-Learning

Sentinel is an efficient tensor management solution on heterogeneous memory for DL workload. Sentinel profiles tensor access at page level, and migrate tensors based on the profiling result.

To use Sentinel, first you need to install profiler in linux-stable. The profiler in Sentinel captures page protection fault every time the page get accessed. 
