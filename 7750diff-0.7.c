// 7750diff v0.7 Beta by Foeh Mannay, March 2013
// Released under the modified BSD license. Please see LICENSE for more information
// For more info on this tool, please visit http://networkbodges.blogspot.com/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct token_s {
        char *text;
        int indent;
} token_t;

typedef struct entity_s {
	char *text;
	int indent;
	struct entity_s *prev;
	struct entity_s *next;
	struct entity_s *parent;
	struct entity_s *child;
} entity;

typedef struct params_s {
	char *file1;
	char *file2;
	int style;
} params_t;

params_t *parseParams(int argc, char *argv[]){
	// Returns a struct with various parameters or NULL if invalid
	unsigned int i = 1;
	params_t *parameters = (params_t*)malloc(sizeof(params_t));

	// There must at least 2 and at most 3 parameters
	if((argc < 3) || (argc > 5)) return(NULL);

	// If we use -first or -second then set the style
	if(argc == 4){
		parameters->style = 0;
		if(strcmp(argv[3], "-first") == 0) parameters->style = 1;
		if(strcmp(argv[3], "-second") == 0) parameters->style = 2;
		if(strcmp(argv[3], "-both") == 0) parameters->style = 3;
		if(parameters->style == 0) return(NULL);
	} else parameters->style = 3;

	parameters->file1 = argv[1];
	parameters->file2 = argv[2];

	return(parameters);

}

char *dewhitespace(char *instring){
// Removes horrible single trailing whitespace the 7750 occasionally leaves
// Feature request 102 for James Leaver
	char *newstring = strdup(instring);
	int pos = strlen(instring) - 1;
	
	while((newstring[pos] == '\n' || newstring[pos] == '\r') && pos >= 0){
		pos--;
	}
	if(newstring[pos] == ' '){
		while(newstring[pos+1] != 0){
			newstring[pos] = newstring[pos+1];
			pos++;
		}
		newstring[pos] = 0;
	}
	
	return(newstring);
}

token_t *nextToken(FILE *inFile){
	int indent = 0;
	char line[1024];
	token_t *token;

	while(fgets(line, sizeof(line), inFile) != NULL){
		indent = 0;
		while(line[indent] == 0x20) indent++;				// Wipe out whitespace
		if(line[indent] == '#') continue;				// Ignore comments
		if(strncmp(&line[indent],"echo",4)==0) continue;		// Ignore echoed messages
		if(strncmp(&line[indent],"exit",4)==0) continue;		// Ignore "exit" and "exit all"
		if((line[indent] == '\n') || (line[indent] == '\r')) continue;	// Ignore empty lines
		
		// If we get here we have a line to return...
		token = (token_t*)malloc(sizeof(token_t));
		if(token == NULL){
			printf("Error allocating memory for token!\n");
			return(NULL);
		}
		token->indent = indent;
		token->text = dewhitespace(&line[indent]);
		
		return(token);
	}
	 return(NULL);
}

void orderedInsert(entity *newElement, entity *baseElement){
	entity *temp;
	
	if(strcmp(newElement->text, baseElement->text) >= 0){
		if(baseElement->next == NULL){
			baseElement->next = newElement;
			newElement->prev = baseElement;
		} else if(strcmp(newElement->text, baseElement->next->text) >= 0){
				orderedInsert(newElement, baseElement->next);
		} else {
			temp = baseElement->next;
			baseElement->next = newElement;
			newElement->prev = baseElement;
			newElement->next = temp;
			temp->prev = newElement;
		}
	} else {
		if(baseElement->prev == NULL){
			baseElement->prev = newElement;
			newElement->next = baseElement;
		} else {
			orderedInsert(newElement, baseElement->prev);
		}
	}
}

entity *insert(entity *newElement, entity *baseElement){
	// This used to be nice and simple but entering multiple times into the same
	// named branch really messed things up, so now it's not!
	entity * cur = baseElement;

	while(cur->prev != NULL){
		cur = cur->prev;
	}
	
	while(strcmp(cur->text, newElement->text) != 0){
		if(cur->next == NULL){
			cur->next = newElement;
			newElement->prev = cur;
			return(newElement);
		}
			
		cur = cur->next;
	}

	free(newElement->text);
	free(newElement);
	
	return(cur);
	
}

void pruneIfMatched(entity *element){
	// prunes off a matched element
	if(element == NULL) return;			// Don't mess with null pointers!

	if(element->child != NULL) return;		// Don't prune branches with unmatched children

	if(element->indent == -1) return;		// Don't prune the root!
	
	if(element->prev != NULL){			// If we have an element to our left
		element->prev->next = element->next;	// point it past us
	} else element->parent->child = element->next;	// otherwise point our parent to our next

	if(element->next != NULL){			// If we have an element to our right
		element->next->prev = element->prev;	// point it past us
	}

	free(element->text);
	free(element);
}

token_t *graft(entity *current, token_t *token, FILE *inFile){
	token_t *returned;
	entity *newEntity;
	
	if(token == NULL) token = nextToken(inFile);			// If we don't have a token, read one
	if(token == NULL) return(NULL);					// If we can't read one then bail
	
	if(token->indent < current->indent) return(token);		// If the token's indent is smaller, return it

	newEntity = (entity*)malloc(sizeof(entity));			// Create a new entity
	newEntity->text = token->text;
	newEntity->indent = token->indent;
	newEntity->prev = NULL;
	newEntity->next = NULL;
	newEntity->child = NULL;
	
	if(token->indent == current->indent) {
		newEntity = insert(newEntity, current);
		newEntity->parent = current->parent;
		returned = graft(newEntity, NULL, inFile);
	} else {
		if(current->child == NULL){
			current->child = newEntity;
		} else{
			newEntity = insert(newEntity, current->child);
		}
		newEntity->parent = current;
		returned = graft(newEntity, NULL, inFile);
	}

	free(token);	
	return(graft(newEntity, returned, inFile));
} 

entity *buildTreeFrom(FILE *inFile){
	entity *root = (entity*)malloc(sizeof(entity));
	
	
	root->text = strdup("");
	root->indent = -1;
	root->parent = NULL;
	root->prev = NULL;
	root->next = NULL;
	root->child = NULL;
	
	graft(root, NULL, inFile);
	
	return(root);
}

void walk(entity *current){
	int i;
	entity *ptr = current;
	if(current == NULL) return;
	while(ptr->prev != NULL){
		ptr = ptr->prev;
	}
	
	do{
		for(i = 0; i++ < ptr->indent; printf(" "));
		printf("%s",ptr->text);
		walk(ptr->child);
		
		if(ptr->next == NULL && ptr->indent > 0){
			for(i = 0; i++ < ptr->parent->indent; printf(" "));
			printf("exit\n");
		}
		ptr = ptr->next;
	} while(ptr != NULL);
}

void diffwalk(entity *left, entity *right){
	// Compares one tree to another
	entity *currentLeft = left;
	entity *currentRight = right;
	entity *tempL, *tempR;
	
	if((left == NULL) || (right == NULL)){
		return;
	}
	
	
	while(currentLeft != NULL){					// Iterate over the siblings on the left
		if(currentRight == NULL) return;
		while(currentRight->prev != NULL){			// Search from the beginning of the right
			currentRight = currentRight->prev;
		}
		while((currentRight->next != NULL) && strcmp(currentLeft->text, currentRight->text) != 0){
			// Scan for a matching node on the right
			currentRight = currentRight->next;
		}
		if(strcmp(currentLeft->text, currentRight->text) == 0){
			// If a match is found... 
			diffwalk(currentLeft->child, currentRight->child);
			
			tempL = currentLeft->next;
			pruneIfMatched(currentLeft);
			currentLeft = tempL;

			if(currentRight->next != NULL){
				tempR = currentRight->next;
			} else {
				tempR = currentRight->prev;
			}
			pruneIfMatched(currentRight);
			currentRight = tempR;
			
		} else {
			currentLeft = currentLeft->next;
		}
	}
}

int main(int argc, char *argv[]){
	entity *root1 = NULL, *root2 = NULL;
	params_t *params = NULL;
	FILE *inputFile = NULL;

	params = parseParams(argc, argv);
	if(params == NULL){
		printf("7750 Configuration Compare Tool v0.7 Beta\n");
		printf("By Foeh Mannay, March 2013\n");
		printf("Released under the modified BSD license. See LICENSE for more details.\n");
		printf("Usage:\n");
		printf("\t%s configfile1 configfile2 [-first|-second|-both]\n\n", argv[0]);
		printf("\twhere \"-first\" and \"-second\" show only the \n\tparts unique to the first or second file.\n");
		return(1);
	}

	inputFile = fopen(params->file1,"r");
	if(inputFile == NULL){
		printf("Error opening %s! Aborting.\n\n",params->file1);
		return(1);
	}
	rewind(inputFile);
	root1 = buildTreeFrom(inputFile);
	fclose(inputFile);

	inputFile = fopen(params->file2,"r");
        if(inputFile == NULL){
                printf("Error opening %s! Aborting.\n\n",params->file2);
                return(1);
        }
	rewind(inputFile);
        root2 = buildTreeFrom(inputFile);
        fclose(inputFile);

	diffwalk(root1, root2);
	if(params->style & 1){
		printf("Unique to %s:\n",argv[1]);
		walk(root1);
	}
	if(params->style & 2){
		printf("Unique to %s:\n",argv[2]);
		walk(root2);
	}
	
	return(0);

}


