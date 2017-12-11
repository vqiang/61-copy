CS 61 Problem Set 6
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information in `AUTHORS.md`.

Race conditions
---------------
Write a SHORT paragraph here explaining your strategy for avoiding
race conditions. No more than 400 words please.

I used mutexes along with conditional variables in multiple places, to prevent racing conditions. The first was in the retry loop in pong_thread, where they were used to block new connections from other threads. Mutexes were also used when updating global variables like n_busy or updating the conn_free linked list. The conditional variable cond_cong also is used to block new requests from all threads when the server is congested. Move_done was also replaced by a conditional variable.


Grading notes (if any)
----------------------



Extra credit attempted (if any)
-------------------------------
