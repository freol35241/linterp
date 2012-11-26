//
// Copyright (c) 2012 Ronaldo Carpio
//                                     
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies and   
// that both that copyright notice and this permission notice appear
// in supporting documentation.  The authors make no representations
// about the suitability of this software for any purpose.          
// It is provided "as is" without express or implied warranty.
//                                                            

#include "linterp.h"
#include <Python.h>
#include <numpy/arrayobject.h>

typedef numeric::ublas::array_adaptor<double> dArrayAdaptor;

struct PythonException {
};

#define bpl_assert(exp, s)	\
	if (!(exp)) {				\
      PyErr_SetString(PyExc_ValueError, (s));	\
      throw PythonException();	\
	}	

// a simple function to adapt a NumPy array to a ublas storage type. Does no reference counting.
template <class T>
numeric::ublas::array_adaptor<T> numpy_array_adaptor(PyObject const *p_obj) {  
  bpl_assert(PyArray_Check(p_obj), "not a numpy array");
  PyArrayObject *p_array_obj = (PyArrayObject*) p_obj;
  bpl_assert(PyArray_ISCONTIGUOUS(p_array_obj), "array must be C-style contiguous");
  bpl_assert(sizeof(T) == PyArray_ITEMSIZE(p_array_obj), "item size does not match");
  return numeric::ublas::array_adaptor<T>(PyArray_Size(p_array_obj), PyArray_BYTES(p_array_obj));
}

// adapts a NumPy array to a boost::multi_array_ref.  number of dimensions is known at compile time
template <int N, class T> 
boost::multi_array_ref<T, N> numpy_multi_array_adaptor(PyObject const *p_obj) {  
  bpl_assert(PyArray_Check(p_obj), "not a numpy array");
  PyArrayObject *p_array_obj = (PyArrayObject*) p_obj;
  bpl_assert(PyArray_ISCONTIGUOUS(p_array_obj), "array must be C-style contiguous");
  bpl_assert(sizeof(T) == PyArray_ITEMSIZE(p_array_obj), "item size does not match");
  bpl_assert(p_array_obj->nd == N, "array dimensions do not match");
  array<int,N> sizes;
  for (int i=0; i<N; i++) {    
    sizes[i] = PyArray_DIM(p_array_obj, i);
  }
  return boost::multi_array_ref<T, N>(PyArray_BYTES(p_array_obj), sizes);
}

// extract a sequence of 1d arrays
vector<dArrayAdaptor> get_darr_sequence(PyObject const *p_sequence) {
  vector<dArrayAdaptor> result;
  PyObject *item;
  int n = PySequence_Length(sequence);
  bpl_assert(
  for (int i=0; i<n; i++) {
    item = PySequence_GetItem(sequence, i);
    result.push_back(numpy_array_adaptor(item));
	Py_DECREF(item); /* Discard reference ownership */
  }
  return result;
}

/*  
template <class Interpolator>
class NDInterpolator_wrap {
public:
  typedef Interpolator interp_type;  
  typedef typename interp_type::value_type T;
  static const int N = interp_type::m_N;
  
  std::unique_ptr<interp_type> m_p_interp_obj;
  
  NDInterpolator_wrap(vector<dPyArr> const &gridList, dPyArr f) {
    const double *grids_begin[N];
	int grids_len[N];
	for (int i=0; i<N; i++)		{ 
	  grids_begin[i] = &(gridList[i][0]); 
	  grids_len[i] = gridList[i].size();
	}
	m_p_interp_obj.reset(new interp_type(grids_begin, grids_len, &f[0], &f[0] + f.size()));
  }
  
  // each dimension is a separate array
  dPyArr interp_vec(vector<dVec> const &coord_arrays) const {
    bpl_assert(coord_arrays.size() == N, "wrong number of coords");
	int n = coord_arrays[0].size();
	for (int i=0; i<N; i++) {
	  bpl_assert(coord_arrays[i].size() == n, "coord arrays have different sizes");
    }	  
	dPyArr result(n);

    clock_t t1 = clock();	
    m_p_interp_obj->interp_vec(n, coord_arrays.begin(), coord_arrays.end(), result.begin());
    clock_t t2 = clock();
	//printf("interp: %d points, %d clocks, %f sec\n", n, (t2-t1), ((double)(t2 - t1)) / CLOCKS_PER_SEC);	
	return result;
  }
};

typedef NDInterpolator_wrap<InterpSimplex<1,double,false,true,dPyArr,dPyArr> > NDInterpolator_1_S_wrap;
typedef NDInterpolator_wrap<InterpSimplex<2,double,false,true,dPyArr,dPyArr> > NDInterpolator_2_S_wrap;
typedef NDInterpolator_wrap<InterpSimplex<3,double,false,true,dPyArr,dPyArr> > NDInterpolator_3_S_wrap;
typedef NDInterpolator_wrap<InterpMultilinear<1,double,false,true,dPyArr,dPyArr> > NDInterpolator_1_ML_wrap;
typedef NDInterpolator_wrap<InterpMultilinear<2,double,false,true,dPyArr,dPyArr> > NDInterpolator_2_ML_wrap;
typedef NDInterpolator_wrap<InterpMultilinear<3,double,false,true,dPyArr,dPyArr> > NDInterpolator_3_ML_wrap;

BOOST_PYTHON_MODULE(_linterp_python)
{   
    bpl::class_<NDInterpolator_1_S_wrap, boost::noncopyable>("Interp_1_S", bpl::init<vector<dPyArr>, dPyArr>())
		.def("interp_vec", &NDInterpolator_1_ML_wrap::interp_vec) 
	;
}
*/

template <int N>
PyObject *linterp(PyObject *self, PyObject *args) {  
  PyArrayObject *p_grids, *p_f, *p_xi;
                                                     
  if (!PyArg_ParseTuple(args, "OOO:linterp", &p_grids, &p_f, &p_xi)) {
    return NULL;
  }
  try {
    vector<dArrayAdaptor> grid_list = get_darr_sequence(p_grids);
    boost::multi_array_ref<double, 1> f = numpy_multi_array_adaptor(p_f);
    vector<dArrayAdaptor> xi = get_darr_sequence(p_xi);	
	bpl_assert(grid_list.size() == xi.size(), "dimensions don't match");
  } catch (PythonException ex) {
    return NULL;
  }
  const double* grids_begin[N];
  int grid_len_list[N];
  const double* xi_list[N];
  size_t min_len = xi[0].size();
  for (int i=0; i<N; i++) {
    grids_begin[i] = &(grid_list[i][0]);
    grid_len_list[i] = grids[i].size();
	xi_list[i] = &(xi[i][0]);
	min_len = (xi.size() < min_len) ? xi.size() : min_len;
  }
  int dims[1] = { xi[0].size() };
  PyObject *p_result = PyArray_ZEROS(1, dims, NPY_DOUBLE, 0);
  if (p_result != NULL) {
    double *p_data = PyArray_BYTES(p_result);
    InterpSimplex<N, double, false> interp_obj(grids_begin, gridgrid_len_list, f.origin(), f.origin() + f.num_elements());
    interp_obj.interp_vec(min_len, xi_list.begin(), xi_list.end(), p_data);    
  }
  return p_result;
}


PyMethodDef methods[] = {
  {"linterp1", linterp<1>, METH_VARARGS},
  {"linterp2", linterp<2>, METH_VARARGS},
  {"linterp3", linterp<3>, METH_VARARGS},
  {NULL, NULL},                    
};
  
#ifdef __cplusplus
extern "C" {      
#endif      
      
void initlinterp_python() {
  import_array();        
  (void)Py_InitModule("linterp_python", methods);
}                                            
 
#ifdef __cplusplus
}                 
#endif