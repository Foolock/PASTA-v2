# PASTA-v2

[1/26] Building up codebase.

# Dependencies

+ use git clone --recursive https://github.com/Foolock/PASTA-v2.git to clone the library
+ [taskflow](https://github.com/taskflow/taskflow) 

# Notes

+ \_breakable\_nodes uses std::vector for now. So if user removes the same edges for multiple times it can lead to duplicate breakable nodes. I leave it for performacne but can be switched to std::set in the future. 
