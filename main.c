#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

struct Exam {
	int count_passed_students;
	int student_id;
	int variant;
	bool task_solved;
	int mark;
	char *ans;
};

const char shared_memory_path[] = "shared_memory";
const char exam_teacher_semaphore_name[] = "exam_teacher_semaphore";
const char exam_student_semaphore_name[] = "exam_student_semaphore";

int main(int argc, char* argv[]) {
	int n;
	if (argc < 2) {
		printf("%s\n", "Incorrect number of arguments.");
		return 0;
	} else {
		n = atoi(argv[1]);
	}

	sem_unlink(exam_teacher_semaphore_name);
	sem_unlink(exam_student_semaphore_name);
	sem_t* exam_teacher_semaphore = sem_open(exam_teacher_semaphore_name, O_CREAT, 0600, 1);
	sem_t* exam_student_semaphore = sem_open(exam_student_semaphore_name, O_CREAT, 0600, 1);

	shm_unlink(shared_memory_path);
	int shm = shm_open(shared_memory_path, O_CREAT | O_RDWR, 0600);
	ftruncate(shm, sizeof(struct Exam));

	struct Exam* exam = mmap(NULL, sizeof(struct Exam), PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
	exam->count_passed_students = 0;
	exam->variant = 0;
	exam->mark = 0;
	exam->task_solved = false;
	exam->ans = malloc(1);
        strcpy(exam->ans, "");

	pid_t student_process_id = -1;
	pid_t teacher_process_id = fork();
	pid_t student_process_ids[n];

	int student_id = -1;
	if (teacher_process_id) {
		for (int i = 0; i < n; ++i) {
			student_process_id = fork();
			if (!student_process_id) {
				student_id = i + 1;
				break;
			}

			student_process_ids[i] = student_process_id;
		}
	}

	if (!teacher_process_id) {
		while (exam->count_passed_students < n) {
			if(exam->variant) {
				sem_wait(exam_teacher_semaphore);			
				printf("Преподаватель отметил, что студент %d вытянул билет %d\n",
					exam->student_id,
					exam->variant);
				exam->variant = 0;
				while (!exam->task_solved) {
					sleep(0.1);
				}
				printf("Преподаватель получил ответ студента %d и начал проверять работу\n",
					exam->student_id);
					
				exam->mark = rand() % 10 + 1;
				printf("Преподаватель поставил студенту %d оценку %d\n",
					exam->student_id,
					exam->mark);
			}
		}
		
		kill(teacher_process_id, SIGTERM);
		for (int i = 0; i < n; ++i) {
			kill(student_process_ids[i], SIGTERM);
		}

		waitpid(teacher_process_id, 0, 0);
		for (int i = 0; i < n; ++i) {
			waitpid(student_process_ids[i], 0, 0);
		}


		shm_unlink(shared_memory_path);
		sem_unlink(exam_student_semaphore_name);
		sem_unlink(exam_teacher_semaphore_name);
		munmap(exam, sizeof(struct Exam));
		return 0;
	}

	if (!student_process_id) {
		sem_wait(exam_student_semaphore);
		exam->student_id = student_id;
		printf("Студент %d заходит в аудиторию\n", student_id);
		srand(time(NULL));
		exam->variant = rand() % 30 + 1;
		printf("Студент %d вытягивает билет %d\n", student_id, exam->variant);
		sem_post(exam_teacher_semaphore);
		char ans[] = "*answer*";
		exam->ans = malloc(strlen(ans)+1);
        	strcpy(exam->ans, ans);
        	while (exam->variant != 0) {
        		sleep(0.1);
        	}
		printf("Студент %d решил билет и передал решение преподавателю\n", student_id);
		exam->task_solved = true;
		while (exam->mark == 0) {
			sleep(0.1);
		}
		printf("Студент %d увидел в зачётке оценку %d и вышел из аудитории\n\n", 
			student_id,
			exam->mark);
		++exam->count_passed_students;
		exam->ans = malloc(1);
        	strcpy(exam->ans, "");
        	exam->mark = 0;	
        	exam->task_solved = false;	
        	sleep(3);
		sem_post(exam_student_semaphore);

	}

	if (teacher_process_id && student_process_id) { // main process
		char key = getchar();
		if (key == 'q') {
			kill(teacher_process_id, SIGTERM);
			for (int i = 0; i < n; ++i) {
				kill(student_process_ids[i], SIGTERM);
			}
		}

		waitpid(teacher_process_id, 0, 0);
		for (int i = 0; i < n; ++i) {
			waitpid(student_process_ids[i], 0, 0);
		}


		shm_unlink(shared_memory_path);
		sem_unlink(exam_student_semaphore_name);
		sem_unlink(exam_teacher_semaphore_name);
		munmap(exam, sizeof(struct Exam));
		return  0;
	}

	return 0;
}
