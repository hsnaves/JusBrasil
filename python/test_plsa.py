import random
import plsa
import numpy as np

if __name__ == "__main__":
	tol = 1e-3
	max_iter = 1000
	num_documents = 4
	num_words = 9
	num_topics = 3

	num_pairs = 500
	documents = np.zeros(num_pairs, dtype=np.int32)
	words = np.zeros(num_pairs, dtype=np.int32)
	weights = np.zeros(num_pairs, dtype=np.float)
	for i in range(num_pairs):
		documents[i] = random.randint(0, num_documents - 1)
		words[i] = random.randint(0, num_words - 1)
		weights[i] = random.randint(1, 4)

	DT, TW = plsa.plsa_train(words, documents, weights, num_words,
	                         num_documents, num_topics, tol, max_iter)
