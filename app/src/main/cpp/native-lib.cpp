#include <jni.h>
#include <string>
#include <sys/prctl.h>
#include <android/log.h>
#include <sys/mman.h>
#include <stdio.h>
#include <signal.h>
#include <sys/auxv.h>
#include <random>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-lib::", __VA_ARGS__))

unsigned char volatile *buffer;
bool isMteOff(){

    if(!((getauxval(AT_HWCAP2)) & HWCAP2_MTE)){
//        LOGI("MTE is off");
        return true;
    }

    return prctl(PR_SET_TAGGED_ADDR_CTRL,
          PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC | (0xfffe << PR_MTE_TAG_SHIFT),
          0, 0, 0);
}
void *mte_alloc(size_t size)
{
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_MTE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap failed");
        return NULL;
    }
    return ptr;
}

uintptr_t set_specific_tag(uintptr_t ptr, uint8_t tag)
{
    // Clear the existing tag and apply the new one
    return (ptr & ~(0xFUL << 56)) | ((uintptr_t)tag << 56);
}

/// @brief Generates a tagged pointer with the specified tag
/// @param ptr The pointer we want to tag
/// @param tag The tag to tag the pointer with
/// @param size How many bytes to tag
void *mte_set_tag(void *ptr, uint8_t tag, size_t size)
{
    uintptr_t base_addr = set_specific_tag((uintptr_t)ptr, tag);
    size_t mte_granule_size = 16;

    // Tag each block in the memory region
    for (size_t offset = 0; offset < size; offset += mte_granule_size)
    {
        uintptr_t current_addr = base_addr + offset;
        asm volatile("stg %0, [%0]" : : "r"(current_addr) : "memory");
    }

    return (void *)base_addr;
}

/// @brief Allocate memory and tag with the supplied tag
/// @param size
/// @param tag The color we want to tag the allocated memory with. See
/// @return
void *mte_malloc(size_t size, uint8_t tag)
{
    if(isMteOff()){
//        LOGI("Running without MTE :((");
        return mmap(NULL, size, PROT_READ | PROT_WRITE , MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    void *ptr = mte_alloc(size);
    if (ptr)
    {
        ptr = mte_set_tag(ptr, tag, size);

        return ptr;
    }

    return NULL;
}

void mte_free(void *ptr, size_t size)
{
    if(isMteOff()){
        munmap(ptr,size);
        return;
    }
    if (ptr != NULL)
    {

        // this will crush any subsequen access to this mem location by other pointers with the previous tag.
        ptr = mte_set_tag(ptr, 0x0, size);

        // Now free the memory
        munmap(ptr,size);
        return;
    }
}

extern "C" JNIEXPORT jstring
JNICALL
Java_com_example_mte_1demo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
        Java_com_example_mte_1demo_MainActivity_hasMTE(JNIEnv *env,jobject /* this */){
    return !isMteOff()? env->NewStringUTF("Running with MTE") :
           env->NewStringUTF("Running without MTE");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mte_1demo_MainActivity_UserAfterFree( JNIEnv *env,jobject /* this */){
    printf("Hello from native C mte bug\n");
    char * volatile ptr = (char *)malloc(32);

    ptr[1] = 'a';

    ptr[33] = 'b';

}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_mte_1demo_MainActivity_allocateMemory(JNIEnv *env, jobject thiz, jlong elements){
    double times[10];
    for(int i = 0;i<10;i++){
        size_t size = 16 * elements;
        LOGI("Allocating %d size",size);
        auto start = std::chrono::high_resolution_clock::now();
        buffer = (unsigned char *)mte_malloc(size,0x02);
        auto end = std::chrono::high_resolution_clock::now();

        LOGI("Buffer pointer: %p",buffer);
        mte_free((void *)buffer,size);
        //This use-after-free is not detected on a Snapdragon 855 whereas on the Tensor G3 we crash
        //here
//        buffer[9] = 9;
        std::chrono::duration<double, std::milli> elapsed=end-start;
//        LOGI("Time elapsed %f",elapsed.count());

        times[i] = elapsed.count();
    }

    double avg_time = 0;
    for(int i = 0;i<10;i++){
        avg_time+=times[i];
    }
//    LOGI("Time elapsed on average: %f",avg_time/10.0);
    return avg_time/10.0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_mte_1demo_MainActivity_fillBuffer(JNIEnv *env, jobject thiz, jlong elements){
    size_t size = 16 * elements;
    prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
    double times[10];

    for(int i = 0;i<10;i++){
//        LOGI("Filling %d bytes",size);
        buffer = (unsigned char*)mte_malloc(size,0x02);
        auto start = std::chrono::high_resolution_clock::now();
        for(size_t i=0;i<size;i++){
            buffer[i] = ('a' + i) % 'z';
        }
        auto end = std::chrono::high_resolution_clock::now();
        mte_free((void *)buffer,size);
        std::chrono::duration<double, std::milli> elapsed=end-start;
//        LOGI("Time elapsed %f",elapsed.count());

        times[i] = elapsed.count();
    }
    double avg_time = 0;
    for(int i = 0;i<10;i++){
        avg_time+=times[i];
    }
//    LOGI("Time elapsed on average: %f",avg_time/10.0);
    return avg_time/10.0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_mte_1demo_MainActivity_fillBufferRandom(JNIEnv *env, jobject thiz, jlong elements){
    size_t size = 16 * elements;
    prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
    double times[10];

    std::random_device rd;  // Non-deterministic random number generator
    std::mt19937 gen(rd());  // Seed the generator
    std::uniform_int_distribution<> distrib(0, size - 1);  // Define the range

    for(int i = 0;i<10;i++){
        LOGI("Filling %d bytes",size);
        buffer = (unsigned char*)mte_malloc(size,0x02);
        auto start = std::chrono::high_resolution_clock::now();
        // Access the memory in a random pattern
        for(size_t j = 0; j < size; j++){
            // Generate a random index to access
            size_t random_index = distrib(gen);
            // Write to the randomly selected index
            buffer[random_index] = ('a' + random_index) % 'z';
        }
        auto end = std::chrono::high_resolution_clock::now();
        mte_free((void *)buffer,size);
        std::chrono::duration<double, std::milli> elapsed=end-start;
//        LOGI("Time elapsed %f",elapsed.count());

        times[i] = elapsed.count();
    }
    double avg_time = 0;
    for(int i = 0;i<10;i++){
        avg_time+=times[i];
    }
//    LOGI("Time elapsed on average: %f",avg_time/10.0);
    return avg_time/10.0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_mte_1demo_MainActivity_traverseBufferRandom(JNIEnv *env, jobject thiz, jlong elements){
    size_t size = 16 * elements;
    prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
    double times[1];

    std::random_device rd;  // Non-deterministic random number generator
    std::mt19937 gen(rd());  // Seed the generator
    std::uniform_int_distribution<> distrib(0, size - 1);  // Define the range

    for(int i = 0;i<1;i++){
        LOGI("Filling %d bytes",size);
        buffer = (unsigned char*)mte_malloc(size,0x02);
        memset((void *const) buffer, 'a', size);
        volatile char c;
        auto start = std::chrono::high_resolution_clock::now();
        // Access the memory in a random pattern
        for(size_t j = 0; j < size; j++){
            // Generate a random index to access
            size_t random_index = distrib(gen);
            // Read to the randomly selected index
            c=buffer[random_index];
        }
        auto end = std::chrono::high_resolution_clock::now();
        mte_free((void *)buffer,size);
        std::chrono::duration<double, std::milli> elapsed=end-start;
        LOGI("Read %c",c);

        times[i] = elapsed.count();
    }
    double avg_time = 0;
    for(int i = 0;i<1;i++){
        avg_time+=times[i];
    }
//    LOGI("Time elapsed on average: %f",avg_time/10.0);
    return avg_time;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_mte_1demo_MainActivity_traverseBuffer(JNIEnv *env, jobject thiz, jlong elements){
    size_t size = 16 * elements;

    prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
    double times[10];

    for(int i = 0;i<10;i++){
        LOGI("Traversing %d bytes",size);
        volatile char c;
        buffer = (unsigned char*)mte_malloc(size,0x02);
        memset((void *const) buffer, 'a', size);
        auto start = std::chrono::high_resolution_clock::now();
        for(size_t i=0;i<size;i++){
            c = buffer[i];
        }
        auto end = std::chrono::high_resolution_clock::now();
        LOGI("read %c from buffer",c);
        mte_free((void *)buffer, size);

        std::chrono::duration<double, std::milli> elapsed=end-start;
//        LOGI("Time elapsed %f",elapsed.count());

        times[i] = elapsed.count();
    }

    double avg_time = 0;
    for(int i = 0;i<10;i++){
        avg_time+=times[i];
    }
//    LOGI("Time elapsed on average: %f",avg_time/10.0);
    return avg_time/10.0;
}


