import numpy as np
import cv2 as cv

img = cv.imread('baboon.png') #original
img2 = cv.imread('bolinhas.png') #original
imgResult = img.copy()
imgResult2 = img2.copy()
imgResult3 = img.copy()
imgResult4 = img2.copy()

print("\n\n\n\n\n\n\n\n\nAtributos da imagem IMG Shape",img.shape,"\n\n\n\n\n\n") #Vai retornar largura e altura e quantidade de canais de cor

for i in range(img.shape[0]): #percorre linhas
    for j in range(img.shape[1]): #percorre colunas
        media = (img.item(i,j,0) + img.item(i,j,1) + img.item(i,j,2))/3.0
        imgResult.__setitem__((i,j,0), img.item(i,j,2)) # canal B ... opencv não usa a ordem rgb, e sim, bgr ... em (i,j,2) está fazendo com que fique branco o vermelho, caso queira o azul branco, colocar 0, e caso queira verde branco coloca 1
        imgResult.__setitem__((i,j,1), img.item(i,j,2)) # canal G
        imgResult.__setitem__((i,j,2), img.item(i,j,2)) # canal R
        imgResult3.__setitem__((i,j,0), media) # canal B ... opencv não usa a ordem rgb, e sim, bgr ... em (i,j,2) está fazendo com que fique branco o vermelho, caso queira o azul branco, colocar 0, e caso queira verde branco coloca 1
        imgResult3.__setitem__((i,j,1), media) # canal G
        imgResult3.__setitem__((i,j,2), media) # canal R

for i in range(img2.shape[0]): #percorre linhas
    for j in range(img2.shape[1]): #percorre colunas
        media2 = (img2.item(i,j,0) * 0.07) + (img2.item(i,j,1) * 0.71) + (img2.item(i,j,2) * 0.21)
        imgResult2.__setitem__((i,j,0), img2.item(i,j,2)) # canal B ... opencv não usa a ordem rgb, e sim, bgr
        imgResult2.__setitem__((i,j,1), img2.item(i,j,2)) # canal G
        imgResult2.__setitem__((i,j,2), img2.item(i,j,2)) # canal R
        imgResult4.__setitem__((i,j,0), media2) # canal B ... opencv não usa a ordem rgb, e sim, bgr
        imgResult4.__setitem__((i,j,1), media2) # canal G
        imgResult4.__setitem__((i,j,2), media2) # canal R

cv.imshow("Imagem Original", img)
cv.imshow("Imagem Original", img2)
cv.imshow("Imagem Aplicado o filtro desejado", imgResult)
cv.imshow("Imagem Aplicado o filtro desejado 2", imgResult2)
cv.imshow("Imagem usando tons de cinza", imgResult3)
cv.imshow("Imagem usando tons de cinza ponderado", imgResult4)

k = cv.waitKey(0)