# Indicate if your solution is deadlock- and starvation-free and explain why.

Starvation occurs when a processes that is a higher priority continuously gets executed first, so lower priority processes get blocked from ever executing. My solution is starvation free, as it uses a mutex locks to promote mutual exclusion. As long as the mutex is locked, only  processes can access shared data at a time. I have all my shared data between mutex locks, therefore my program should be free from race conditions such as starvation. 

A deadlock occurs when no processes are capable of proceeding, as they are waiting for each other to release some resource. My solutions is deadlock free when num_guides is equal to one. Each test case which only has one guide always completes, regardless of the number of visitors. This is likely because the one guide process does not need to wait for any other guide processes to proceed. 

My solution is also deadlock free when num_guides is equal to two. The program executes with all processes properly proceeding. The risk of a deadlock occurring increases with two guides because the guide processes must not only wait for all the visitor processes to leave, but they must wait for each other to leave. Luckily, the guide processes in my program properly wait for each other and do not cause any deadlocks.

When increasing the number of guides to three, a deadlock occurs in my solution. In one instance that I ran my program with num_guide being 3, once the first two guides leave the museum, the third guide seems to wait forever and never enters the museum. It would seem that the third guide is waiting for some resource to be released so it can stop waiting. This would be a deadlock.
