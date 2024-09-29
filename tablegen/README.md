# TableGen 

Here we use our custom generator to generate keywords and filterers from the offical c_lang table defintion taken from LLVM


### Note:
Using a plain binary search for the keyword filter does not give a better performance than the implementation based on the llvm::StringMap type in aman-lang. 

To beat the performance of the current implementation, you need to generate a perfect hash function(hash function that knows all the keys already so no collision) like one of the gnu hash generator i forgot the name...

This paper is considered state of the art as well: https://arxiv.org/pdf/2104.10402.pdf.