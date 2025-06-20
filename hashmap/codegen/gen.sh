#!/bin/sh

types="
	int64_t
	int32_t
	int16_t
	int8_t
	uint64_t
	uint32_t
	uint16_t
	uint8_t
	float
	double
"

for t in $types; do
	postfix=$(echo "$t" | sed 's/_t//g')

	echo $postfix

	cat > "hashmap-$postfix.c" <<- EOM
		#define HASHMAP_PREFIX $postfix
		#define HASHMAP_VAL $t
		#include "hashmap.c"
	EOM

	cat > "hashmap-$postfix.h" <<- EOM
		#pragma once
		#define HASHMAP_PREFIX $postfix
		#define HASHMAP_VAL $t
		#include "hashmap.h"
	EOM
done

