
/**

run: ./color cs112.html http://www.cs.cmu.edu/~dga/15-441/S08/lectures/03-socket.pdf https://christian-rossow.de/publications/amplification-ndss2014.pdf:448 http://www.microsoft.com/en-us/research/wp-content/uploads/2017/01/dctcp-sigcomm2010.pdf

Test with the cs112 website. 

cs112 links:
http://www.cs.cmu.edu/~dga/15-441/F08/lectures/05-physical.pdf

http://ccr.sigcomm.org/online/files/p83-keshavA.pdf

https://www.microsoft.com/en-us/research/wp-content/uploads/2010/01/catnap-mobisys.pdf

http://people.csail.mit.edu/rahul/papers/cope-sigcomm2006.pdf

https://christian-rossow.de/publications/amplification-ndss2014.pdf


http://conferences.sigcomm.org/sigcomm/2013/papers/sigcomm/p159.pdf

https://www.microsoft.com/en-us/research/wp-content/uploads/2017/01/dctcp-sigcomm2010.pdf


http://conferences.sigcomm.org/co-next/2009/papers/Jacobson.pdf


*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h> // oflags
#include <string.h>
#include "http.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: ./color <filename> [url1, ...]\n");
        return 1;
    }

    fprintf(stderr, "Given %d keys\n", argc - 2);

    // Open the source file
    char *sourceFilename = argv[1];
    struct stat stats;
    stat(sourceFilename, &stats);
    size_t file_size_bytes = stats.st_size;
    int in_file = open(sourceFilename, O_RDONLY);

    // Create a buffer for the file 
    char *buffer = calloc(file_size_bytes + 1, sizeof(char));
    read(in_file, buffer, file_size_bytes);

    // Display original 
    // fprintf(stderr, "+========================== ORIGINAL ==========================+\n");
    // fprintf(stderr, "%s\n", buffer);
    fprintf(stderr, "original buffer size: %d\n", file_size_bytes);
    // fprintf(stderr, "+========================== END ORIGINAL ==========================+\n");

    color_links(&buffer, &file_size_bytes, &argv[2], argc - 2);

    // fprintf(stderr, "+========================== New ==========================+\n");
    // fprintf(stderr, "%s\n", buffer);
    fprintf(stderr, "new buffer size: %d\n", file_size_bytes);
    // fprintf(stderr, "+========================== END New ==========================+\n");


    // Write results to out file
    int out_file = open("ColorCs112.html", O_WRONLY | O_CREAT, 0777); 
    write(out_file, buffer, file_size_bytes);
    fprintf(stderr, "Wrote to file\n");


    close(in_file);
    close(out_file);
    free(buffer);


    return 0;
}