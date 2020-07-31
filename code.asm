	mov dx,0x3f8
lab1:
	mov bx,0x2000
	mov al,[bx]
	out dx,al

	inc bx
	mov al,[bx]
	out dx,al
	
	mov al,10
	out dx,al
	
	jmp lab1
	hlt
