# Phase 1: 기본 셸 기능 구현

## 개요

이 단계에서는 사용자의 입력을 받아 기본적인 명령어를 실행하는 myshell을 구현함  
내장 명령어 처리, 외부 프로그램 실행, 백그라운드 실행 기능 등을 포함함

## 구현 기능

- `cd`, `exit`, `quit` 등 내장 명령어 지원
- `ls`, `cat`, `mkdir` 등의 외부 명령 실행
- `&` 기호를 통한 백그라운드 프로세스 실행
- `SIGCHLD` 핸들러를 통한 자식 프로세스 종료 처리

## 주요 파일 설명

- `myshell.c`: 셸의 주요 기능 구현 (입력 처리, 명령 실행, 시그널 처리 등)
- `myshell.h`: 셸에 필요한 상수, 함수 프로토타입 정의
- `csapp.c`, `csapp.h`: 시스템 호출에 대한 에러 핸들링 및 래퍼 함수 제공
- `Makefile`: `myshell` 실행 파일 생성용 빌드 설정 파일

## 사용한 시스템 호출

- `fork()`, `execvp()`, `waitpid()`, `signal()`, `chdir()` 등
- wrapper 함수 사용: `Fork()`, `Execve()`, `Waitpid()` 등은 `csapp.c` 기반

## 빌드 방법 및 실행 방법

```bash
make

으로 빌드 후에 

./myshell

로 실행 가능함