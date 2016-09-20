# -*- coding: utf-8 -*-

import numpy as np
cimport numpy as np
import math

DTYPE = np.float
ctypedef np.float_t DTYPE_t

cdef generate_random_matrix(int num_rows, int num_cols):
	'''Generates a uniformly random matrix whose columns add up to 1.

	Args:
		num_rows (int): number of rows of the matrix.
		num_col (int): number of columns of the matrix.

	Returns:
		The random matrix.
	'''
	cdef np.ndarray[DTYPE_t, ndim=2] M = \
	    np.random.uniform(size = (num_rows, num_cols), dtype=DTYPE)
	cdef np.ndarray[DTYPE_t, ndim=2] MM = -np.log(M)
	return MM * np.reciprocal(np.sum(MM, axis = 0))

def plsa_iteration(int[:] words, int[:] documents, DTYPE_t[:] weights,
                   int num_topics, np.ndarray[DTYPE_t, ndim=2] DT,
                   np.ndarray[DTYPE_t, ndim=2] TW,
                   np.ndarray[DTYPE_t, ndim=2] DT2,
                   np.ndarray[DTYPE_t, ndim=2] TW2):
	'''One E-M iteration of the PLSA algorithm.

	This function is an auxiliary function to `plsa_train`.
	It implements the both the E- and the M-step of the PLSA algorithm.
	Here DT represents the matrix of conditional probabilities for
	topics given the document, that is DT[i,j] = Prob[topic = i|doc = j].
	Similarly TW is the matrix of conditional probabilities for the
	word given the topic, that is TW[i, j] = Prob[word = i|topic = j].
	The matrix DT2 is where we compute the new value of the matrix
	DT for the next iteration and TW2 is where we store the new value
	for the TW matrix.

	Args:
		words (array of int): the list of words in the occurrences.
		documents (array of int): the list of documents in the
			occurrences.
		weights (array of int): the weight of the occurrence.
		num_topics (int): the number of topics, which is
			the number of possible values of the latent variable.
		DT (numpy.array): the matrix of conditional probabilities
			for topics given the document
		TW (numpy.array): the matrix of conditional probabilities
			for word given the topic.
		DT2 (numpy.array): matrix where we will store the
			new conditional probabilities for topic given document.
		TW2 (numpy.array): matrix where we will store the
			new conditional probabilities for word given topic.
	Returns:
		The average log-likelihood of this iteration.
	'''

	cdef DTYPE_t likelihood = 0.0
	cdef DTYPE_t total_weight = 0.0
	cdef int word, document
	cdef DTYPE_t w, dotprod, val
	cdef int idx, topic

	DT2.fill(0.0)
	TW2.fill(0.0)
	for idx in range(words.shape[0]):
		word = words[idx]
		document = documents[idx]
		w = weights[idx]

		dotprod = np.dot(DT[document, :], TW[:, word])
		likelihood += w * math.log(dotprod)
		total_weight += w

		for topic in range(num_topics):
			val =w * DT[document, topic] * \
				TW[topic, word] / dotprod
			DT2[document, topic] += val
			TW2[topic, word] += val

	DT2 = DT2 * np.reciprocal(np.sum(DT, axis = 0))
	TW2 = TW2 * np.reciprocal(np.sum(DT, axis = 0))

	return likelihood / total_weight

def plsa_train(int[:] words, int[:] documents, DTYPE_t[:] weights,
               int num_words, int num_documents, int num_topics = 10,
               DTYPE_t tol = 1e-4, int max_iter = 300):
	'''PLSA algorithm.

	Args:
		words (array of int): the list of words in the occurrences.
		documents (array of int): the list of documents in the
			occurrences.
		weights (array of int): the weight of the occurrence.
		num_words (int): the number of distinct words.
		num_documents (int): the number of distict documents.
		num_topics (int): the number of topics, which is
			the number of possible values of the latent variable.
		tol (float): the tolerance criterion. We stop
			the iterations once the average log-likelihood
			changes by at most `tol'.
		max_iter (int): the maximum number of iterations
			of the algorithm

	Returns:
		Two matrices, DT and TW of conditional probabilities
	'''
	# Generates random matrices as initial conditional probabilities
	cdef np.ndarray[DTYPE_t, ndim=2] DT1 = \
	    generate_random_matrix(num_documents, num_topics)
	cdef np.ndarray[DTYPE_t, ndim=2] TW1 = \
	    generate_random_matrix(num_topics, num_words)

	cdef np.ndarray[DTYPE_t, ndim=2] DT2 = \
	    np.zeros((num_documents, num_topics), dtype=DTYPE)
	cdef np.ndarray[DTYPE_t, ndim=2] TW2 = \
	    np.zeros((num_topics, num_words), dtype=DTYPE)

	cdef DTYPE_t old_likelihood = -100.0 * tol
	cdef DTYPE_t likelihood
	cdef int it = 0

	while it < max_iter:
		likelihood = plsa_iteration(words, documents, weights,
		                            num_topics, DT1, TW1, DT2, TW2)

		DT1, DT2 = DT2, DT1
		TW1, TW2 = TW2, TW1

		it += 1
		print "Iteration %d, likelikhood = %f" % (it, likelihood)

		if math.fabs(old_likelihood - likelihood) < tol:
			break
		old_likelihood = likelihood

	return DT1, TW1


