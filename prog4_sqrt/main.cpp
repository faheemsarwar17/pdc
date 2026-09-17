#include <stdio.h>
#include <algorithm>
#include <pthread.h>
#include <math.h>
#include <cstdlib>
#include <string>

#include "CycleTimer.h"
#include "sqrt_ispc.h"

using namespace ispc;

extern void sqrtSerial(int N, float startGuess, float* values, float* output);

static int iterationCount(float value, float initialGuess) {
    const float threshold = 0.00001f;
    float guess = initialGuess;
    float error = fabs(guess * guess * value - 1.f);
    int iterations = 0;
    while (error > threshold) {
        guess = (3.f * guess - value * guess * guess * guess) * 0.5f;
        error = fabs(guess * guess * value - 1.f);
        iterations++;
    }
    return iterations;
}

static void verifyResult(int N, float* result, float* gold) {
    for (int i=0; i<N; i++) {
        if (fabs(result[i] - gold[i]) > 1e-4) {
            printf("Error: [%d] Got %f expected %f\n", i, result[i], gold[i]);
        }
    }
}

int main() {

    const unsigned int N = 20 * 1000 * 1000;
    const float initialGuess = 1.0f;

    float* values = new float[N];
    float* output = new float[N];
    float* gold = new float[N];

    const bool worstCase = std::getenv("SQRT_CASE") != nullptr &&
                           std::string(std::getenv("SQRT_CASE")) == "worst";
    for (unsigned int i = 0; i < N; i++) {
        values[i] = worstCase && (i % 8 == 0) ? 0.000001f : 1.0f;
    }

    printf("[sqrt %s lane iterations]:", worstCase ? "worst" : "best");
    for (int lane = 0; lane < 8; lane++) {
        printf(" %d", iterationCount(values[lane], initialGuess));
    }
    printf("\n");

    // generate a gold version to check results
    for (unsigned int i=0; i<N; i++)
        gold[i] = sqrt(values[i]);

    //
    // And run the serial implementation 3 times, again reporting the
    // minimum time.
    //
    double minSerial = 1e30;
    double maxSerial = 0.0;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrtSerial(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
        maxSerial = std::max(maxSerial, endTime - startTime);
    }

    printf("[sqrt serial]:\t\t[%.3f] ms (min %.3f, max %.3f)\n", minSerial * 1000, minSerial * 1000, maxSerial * 1000);

    verifyResult(N, output, gold);

    //
    // Compute the image using the ispc implementation; report the minimum
    // time of three runs.
    //
    double minISPC = 1e30;
    double maxISPC = 0.0;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrt_ispc(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
        maxISPC = std::max(maxISPC, endTime - startTime);
    }

    printf("[sqrt ispc]:\t\t[%.3f] ms (min %.3f, max %.3f)\n", minISPC * 1000, minISPC * 1000, maxISPC * 1000);

    verifyResult(N, output, gold);

    // Clear out the buffer
    for (unsigned int i = 0; i < N; ++i)
        output[i] = 0;

    //
    // Tasking version of the ISPC code
    //
    double minTaskISPC = 1e30;
    double maxTaskISPC = 0.0;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrt_ispc_withtasks(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minTaskISPC = std::min(minTaskISPC, endTime - startTime);
        maxTaskISPC = std::max(maxTaskISPC, endTime - startTime);
    }

    printf("[sqrt task ispc]:\t[%.3f] ms (min %.3f, max %.3f)\n", minTaskISPC * 1000, minTaskISPC * 1000, maxTaskISPC * 1000);

    verifyResult(N, output, gold);

    printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);

    delete [] values;
    delete [] output;
    delete [] gold;

    return 0;
}
