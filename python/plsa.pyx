# -*- coding: utf-8 -*-

import numpy as np
cimport numpy as np
import math

DTYPE = np.float
ctypedef np.float_t DTYPE_t

cdef normalize_rows(np.ndarray[DTYPE_t, ndim=2] A):
	'''Normalize rows so that each row adds up to 1.

	Args:
		A (numpy.array): the matrix to be normalized.
	'''
	cdef np.ndarray[DTYPE_t, ndim = 1] sums = np.sum(A, axis = 1)
	A /= np.expand_dims(sums, axis = 1)

cdef generate_random_matrix(int num_rows, int num_cols):
	'''Generates a uniformly random matrix whose columns add up to 1.

	Args:
		num_rows (int): number of rows of the matrix.
		num_col (int): number of columns of the matrix.

	Returns:
		The random matrix.
	'''
	cdef np.ndarray[DTYPE_t, ndim = 2] M = \
	    np.random.uniform(size = (num_rows, num_cols))
	cdef np.ndarray[DTYPE_t, ndim = 2] MM = -np.log(M)

	normalize_rows(MM)
	return MM

def plsa_iteration(int[:] words, int[:] documents, DTYPE_t[:] weights,
                   int num_topics, np.ndarray[DTYPE_t, ndim=2] DT,
                   np.ndarray[DTYPE_t, ndim = 2] TW,
                   np.ndarray[DTYPE_t, ndim = 2] DT2,
                   np.ndarray[DTYPE_t, ndim = 2] TW2):
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

	if not DT2 is None:
		DT2.fill(0.0)
	if not TW2 is None:
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
			if not DT2 is None:
				DT2[document, topic] += val
			if not TW2 is None:
				TW2[topic, word] += val

	if not DT2 is None:
		normalize_rows(DT2)
	if not TW2 is None:
		normalize_rows(TW2)

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
	cdef np.ndarray[DTYPE_t, ndim = 2] DT1 = \
	    generate_random_matrix(num_documents, num_topics)
	cdef np.ndarray[DTYPE_t, ndim = 2] TW1 = \
	    generate_random_matrix(num_topics, num_words)

	cdef np.ndarray[DTYPE_t, ndim=2] DT2 = \
	    np.zeros((num_documents, num_topics), dtype=DTYPE)
	cdef np.ndarray[DTYPE_t, ndim=2] TW2 = \
	    np.zeros((num_topics, num_words), dtype=DTYPE)

	cdef DTYPE_t old_likelihood = -100.0 * tol
	cdef DTYPE_t likelihood
	cdef int it = 0

	print "PLSA traininig..."
	while it < max_iter:
		likelihood = plsa_iteration(words, documents, weights,
		                            num_topics, DT1, TW1, DT2, TW2)

		DT1, DT2 = DT2, DT1
		TW1, TW2 = TW2, TW1

		it += 1
		print "Iteration %d: likelihood = %f" % (it, likelihood)

		if math.fabs(old_likelihood - likelihood) < tol:
			break
		old_likelihood = likelihood

	return DT1, TW1

def plsa_retrain(int[:] words, int[:] documents, DTYPE_t[:] weights,
                 int num_documents, np.ndarray[DTYPE_t, ndim = 2] TW,
                 DTYPE_t tol = 1e-4, int max_iter = 300):
	'''PLSA retraining algorithm.

	Args:
		words (array of int): the list of words in the occurrences.
		documents (array of int): the list of documents in the
			occurrences.
		weights (array of int): the weight of the occurrence.
		num_documents (int): the number of distict documents.
		TW (numpy.array): the TW matrix returned by `plsa_train`.
		tol (float): the tolerance criterion. We stop
			the iterations once the average log-likelihood
			changes by at most `tol'.
		max_iter (int): the maximum number of iterations
			of the algorithm

	Returns:
		The updated DT matrix.
	'''
	cdef int num_words = TW.shape[1]
	cdef int num_topics = TW.shape[0]
	# Generates random matrices as initial conditional probabilities
	cdef np.ndarray[DTYPE_t, ndim = 2] DT1 = \
	    generate_random_matrix(num_documents, num_topics)

	cdef np.ndarray[DTYPE_t, ndim=2] DT2 = \
	    np.zeros((num_documents, num_topics), dtype=DTYPE)

	cdef DTYPE_t old_likelihood = -100.0 * tol
	cdef DTYPE_t likelihood
	cdef int it = 0

	print "PLSA retraininig..."
	while it < max_iter:
		likelihood = plsa_iteration(words, documents, weights,
		                            num_topics, DT1, TW, DT2, None)

		DT1, DT2 = DT2, DT1

		it += 1
		print "Iteration %d: likelihood = %f" % (it, likelihood)

		if math.fabs(old_likelihood - likelihood) < tol:
			break
		old_likelihood = likelihood

	return DT1
