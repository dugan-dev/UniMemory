cmake_minimum_required(VERSION 3.25)
# Reject the requested profile before resolving SDK/Threads. NONE prevents
# an unrelated compiler failure from masquerading as the intended rejection.
project(UniMemoryConflictingRequest LANGUAGES NONE)
find_package(UniMemory 0.0.1 EXACT CONFIG REQUIRED)
message(FATAL_ERROR "The conflicting installed profile request was accepted")
