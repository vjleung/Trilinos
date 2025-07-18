/*
 * Copyright(C) 1999-2024 National Technology & Engineering Solutions
 * of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
 * NTESS, the U.S. Government retains certain rights in this software.
 *
 * See packages/seacas/LICENSE for details
 */

#include "exodusII.h"     // for EX_FATAL, exerrval, ex_err, etc
#include "exodusII_int.h" // for exi_get_counter_list, etc

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

#define EXCHECK(funcall)                                                                           \
  if ((funcall) != EX_NOERR) {                                                                     \
    fprintf(stderr, "Error calling %s\n", TOSTRING(funcall));                                      \
    EX_FUNC_LEAVE(EX_FATAL);                                                                       \
  }

#define EXCHECKI(funcall)                                                                          \
  if ((funcall) != EX_NOERR) {                                                                     \
    fprintf(stderr, "Error calling %s\n", TOSTRING(funcall));                                      \
    return EX_FATAL;                                                                               \
  }

#define EXCHECKF(funcall)                                                                          \
  if ((funcall) != EX_NOERR) {                                                                     \
    fprintf(stderr, "Error calling %s\n", TOSTRING(funcall));                                      \
    goto err_ret;                                                                                  \
  }

/*! \cond INTERNAL */
struct ncvar
{ /* variable */
  char    name[MAX_VAR_NAME_LENGTH];
  nc_type type;
  int     ndims;
  int     dims[NC_MAX_VAR_DIMS];
  int     natts;
};

struct ncatt
{ /* attribute */
  char    name[MAX_VAR_NAME_LENGTH];
  nc_type type;
  size_t  len;
};

static size_t type_size(nc_type type);
static int    cpy_variable_data(int in_exoid, int out_exoid, int in_large, int mesh_only);
static int    cpy_variables(int in_exoid, int out_exoid, int in_large, int mesh_only);
static int    cpy_dimension(int in_exoid, int out_exoid, int mesh_only);
static int    cpy_global_att(int in_exoid, int out_exoid);
static int    cpy_att(int in_id, int out_id, int var_in_id, int var_out_id);
static int    cpy_var_def(int in_id, int out_id, int rec_dim_id, char *var_nm);
static int    cpy_var_val(int in_id, int out_id, char *var_nm);
static int    cpy_coord_def(int in_id, int out_id, int rec_dim_id, char *var_nm, int in_large);
static int    cpy_coord_val(int in_id, int out_id, char *var_nm, int in_large);
static void   update_structs(int out_exoid);
static void   update_internal_structs(int out_exoid, ex_inquiry inqcode,
                                      struct exi_list_item **ctr_list);

static int is_truth_table_variable(const char *var_name)
{
  /* If copying just the "mesh" or "non-transient" portion of the
   * input DB, these are the variables that won't be copied:
   */
  return strstr(var_name, "_var_tab") != NULL;
}

static int is_non_mesh_variable(const char *var_name)
{
  /* If copying just the "mesh" or "non-transient" portion of the
   * input DB, these are the variables that won't be copied:
   */
  return (strncmp(var_name, "vals_", 5) == 0) || (strncmp(var_name, "name_", 5) == 0);
}
/*! \endcond */

/*! \cond INTERNAL */
static int ex_copy_internal(int in_exoid, int out_exoid, int mesh_only)
{
  EX_FUNC_ENTER();
  if (exi_check_valid_file_id(in_exoid, __func__) != EX_NOERR) {
    EX_FUNC_LEAVE(EX_FATAL);
  }
  if (exi_check_valid_file_id(out_exoid, __func__) != EX_NOERR) {
    EX_FUNC_LEAVE(EX_FATAL);
  }

  /*
   * Get exodus_large_model setting on both input and output
   * databases so know how to handle coordinates.
   */
  int in_large = ex_large_model(in_exoid);

  /*
   * Get integer sizes for both input and output databases.
   * Currently they should both match or there will be an error.
   */
  if (ex_int64_status(in_exoid) != ex_int64_status(out_exoid)) {
    char errmsg[MAX_ERR_LENGTH];
    snprintf_nowarn(errmsg, MAX_ERR_LENGTH,
                    "ERROR: integer sizes do not match for input and output databases.");
    ex_err_fn(in_exoid, __func__, errmsg, EX_WRONGFILETYPE);
    EX_FUNC_LEAVE(EX_FATAL);
  }

  /* put output file into define mode */
  EXCHECK(exi_redef(out_exoid, __func__));

  /* copy global attributes */
  EXCHECK(cpy_global_att(in_exoid, out_exoid));

  /* copy dimensions */
  EXCHECK(cpy_dimension(in_exoid, out_exoid, mesh_only));

  /* copy variable definitions and variable attributes */
  EXCHECK(cpy_variables(in_exoid, out_exoid, in_large, mesh_only));

  /* take the output file out of define mode */
  if (exi_leavedef(out_exoid, __func__) != EX_NOERR) {
    EX_FUNC_LEAVE(EX_FATAL);
  }

  /* output variable data */
  EXCHECK(cpy_variable_data(in_exoid, out_exoid, in_large, mesh_only));

  /* ensure internal data structures are updated */
  update_structs(out_exoid);

  ex_update(out_exoid);

  EX_FUNC_LEAVE(EX_NOERR);
}
/*! \endcond */

/*!
  \ingroup Utilities
  \undoc

 *  efficiently copies all non-transient information (attributes,
 * dimensions, and variables from an opened EXODUS file to another
 * opened EXODUS file.  Will not overwrite a dimension or variable
 * already defined in the new file.
 * \param      in_exoid     exodus file id for input file
 * \param      out_exoid    exodus file id for output file
 */

int ex_copy(int in_exoid, int out_exoid)
{
  int mesh_only = 1;
  return ex_copy_internal(in_exoid, out_exoid, mesh_only);
}

/*!
  \ingroup Utilities
  \undoc

 *  efficiently copies all non-transient and transient information
 * (attributes, dimensions, and variables from an opened EXODUS file
 * to another opened EXODUS file.  Will not overwrite a dimension or
 * variable already defined in the new file.
 * \param     in_exoid     exodus file id for input file
 * \param     out_exoid    exodus file id for output file
 */

int ex_copy_transient(int in_exoid, int out_exoid)
{
  int mesh_only = 0;
  return ex_copy_internal(in_exoid, out_exoid, mesh_only);
}

/*! \cond INTERNAL */
static int cpy_variable_data(int in_exoid, int out_exoid, int in_large, int mesh_only)
{
  int nvars; /* number of variables */
  /* NOTE: This is incorrect for files containing groups */
  EXCHECKI(nc_inq(in_exoid, NULL, &nvars, NULL, NULL));
  for (int varid = 0; varid < nvars; varid++) {
    bool         is_filtered;
    struct ncvar var; /* variable */
    EXCHECKI(nc_inq_var(in_exoid, varid, var.name, &var.type, &var.ndims, var.dims, &var.natts));
    if ((strcmp(var.name, VAR_QA_TITLE) == 0) || (strcmp(var.name, VAR_INFO) == 0)) {
      is_filtered = true;
    }
    else if (is_truth_table_variable(var.name)) {
      is_filtered = true;
    }
    else if (mesh_only == 1 &&
             (is_non_mesh_variable(var.name) || (strcmp(var.name, VAR_WHOLE_TIME) == 0))) {
      is_filtered = true;
    }
    else if (mesh_only == 0 &&
             (!is_non_mesh_variable(var.name) && strcmp(var.name, VAR_WHOLE_TIME) != 0)) {
      is_filtered = true;
    }
    else {
      is_filtered = false;
    }

    if (!is_filtered) {
      if (strncmp(var.name, VAR_COORD, 5) == 0) {
        cpy_coord_val(in_exoid, out_exoid, var.name, in_large);
      }
      else {
        cpy_var_val(in_exoid, out_exoid, var.name);
      }
    }
  }
  return EX_NOERR;
}

/*! \cond INTERNAL */
static int cpy_variables(int in_exoid, int out_exoid, int in_large, int mesh_only)
{
  int recdimid; /* id of unlimited dimension */
  int nvars;    /* number of variables */
  /* NOTE: This is incorrect for files containing groups */
  EXCHECKI(nc_inq(in_exoid, NULL, &nvars, NULL, &recdimid));
  for (int varid = 0; varid < nvars; varid++) {
    struct ncvar var; /* variable */
    EXCHECKI(nc_inq_var(in_exoid, varid, var.name, &var.type, &var.ndims, var.dims, &var.natts));

    bool is_filtered;
    if ((strcmp(var.name, VAR_QA_TITLE) == 0) || (strcmp(var.name, VAR_INFO) == 0)) {
      is_filtered = true;
    }
    else if (is_truth_table_variable(var.name)) {
      is_filtered = true;
    }
    else if (mesh_only == 1 && is_non_mesh_variable(var.name)) {
      is_filtered = true;
    }
    else {
      is_filtered = false;
    }

    if (!is_filtered) {
      int var_out_id; /* variable id */
      if (strncmp(var.name, VAR_COORD, 5) == 0) {
        var_out_id = cpy_coord_def(in_exoid, out_exoid, recdimid, var.name, in_large);
      }
      else {
        var_out_id = cpy_var_def(in_exoid, out_exoid, recdimid, var.name);
      }

      /* copy the variable's attributes */
      cpy_att(in_exoid, out_exoid, varid, var_out_id);
    }
  }
  return EX_NOERR;
}
/*! \endcond */

/*! \cond INTERNAL */
static int cpy_dimension(int in_exoid, int out_exoid, int mesh_only)
{
  int dim_out_id; /* dimension id */

  int ndims;    /* number of dimensions */
  int recdimid; /* id of unlimited dimension */
  /* NOTE: This is incorrect for files containing groups */
  EXCHECKI(nc_inq(in_exoid, &ndims, NULL, NULL, &recdimid));
  for (int dimid = 0; dimid < ndims; dimid++) {

    char   dim_nm[EX_MAX_NAME + 1];
    size_t dim_sz;
    EXCHECK(nc_inq_dim(in_exoid, dimid, dim_nm, &dim_sz));

    /* If the dimension isn't one we specifically don't want
     * to copy (ie, number of QA or INFO records) and it
     * hasn't been defined, copy it */
    bool is_filtered;
    if (strcmp(dim_nm, DIM_NUM_QA) == 0 || strcmp(dim_nm, DIM_NUM_INFO) == 0 ||
        strcmp(dim_nm, DIM_N4) == 0 || strcmp(dim_nm, DIM_STR) == 0 ||
        strcmp(dim_nm, DIM_LIN) == 0) {
      is_filtered = true;
    }
    else if (mesh_only == 1 &&
             ((strcmp(dim_nm, DIM_NUM_NOD_VAR) == 0) || (strcmp(dim_nm, DIM_NUM_EDG_VAR) == 0) ||
              (strcmp(dim_nm, DIM_NUM_FAC_VAR) == 0) || (strcmp(dim_nm, DIM_NUM_ELE_VAR) == 0) ||
              (strcmp(dim_nm, DIM_NUM_NSET_VAR) == 0) || (strcmp(dim_nm, DIM_NUM_ESET_VAR) == 0) ||
              (strcmp(dim_nm, DIM_NUM_FSET_VAR) == 0) || (strcmp(dim_nm, DIM_NUM_SSET_VAR) == 0) ||
              (strcmp(dim_nm, DIM_NUM_ELSET_VAR) == 0) || (strcmp(dim_nm, DIM_NUM_GLO_VAR) == 0) ||
              (strcmp(dim_nm, DIM_NUM_ASSEMBLY_VAR) == 0) ||
              (strcmp(dim_nm, DIM_NUM_BLOB_VAR) == 0) || (strstr(dim_nm, "_red_var") != NULL))) {
      is_filtered = true;
    }
    else {
      is_filtered = false;
    }

    if (!is_filtered) {
      /* See if the dimension has already been defined */
      int status = nc_inq_dimid(out_exoid, dim_nm, &dim_out_id);

      if (status != EX_NOERR) {
        if (dimid != recdimid) {
          status = nc_def_dim(out_exoid, dim_nm, dim_sz, &dim_out_id);
        }
        else {
          status = nc_def_dim(out_exoid, dim_nm, NC_UNLIMITED, &dim_out_id);
        }
        if (status != EX_NOERR) {
          char errmsg[MAX_ERR_LENGTH];
          snprintf_nowarn(errmsg, MAX_ERR_LENGTH,
                          "ERROR: failed to define %s dimension in file id %d", dim_nm, out_exoid);
          ex_err_fn(out_exoid, __func__, errmsg, status);
          EX_FUNC_LEAVE(EX_FATAL);
        }
      }
    }
  }

  /* DIM_STR_NAME is a newly added dimension required by current API.
   * If it doesn't exist on the source database, we need to add it to
   * the target...
   */
  int status = nc_inq_dimid(in_exoid, DIM_STR_NAME, &dim_out_id);
  if (status != EX_NOERR) {
    /*
     * See if it already exists in the output file
     * (ex_put_init_ext may have been called on the
     * output file prior to calling ex_copy)
     */
    status = nc_inq_dimid(out_exoid, DIM_STR_NAME, &dim_out_id);
    if (status != EX_NOERR) {
      /* Not found; set to default value of 32+1. */

      if ((status = nc_def_dim(out_exoid, DIM_STR_NAME, 33, &dim_out_id)) != EX_NOERR) {
        char errmsg[MAX_ERR_LENGTH];
        snprintf_nowarn(errmsg, MAX_ERR_LENGTH,
                        "ERROR: failed to define string name dimension in file id %d", out_exoid);
        ex_err_fn(out_exoid, __func__, errmsg, status);
        EX_FUNC_LEAVE(EX_FATAL);
      }
    }
  }
  return EX_NOERR;
}

/*! \cond INTERNAL */
static int cpy_global_att(int in_exoid, int out_exoid)
{
  struct ncatt att; /* attribute */

  int ngatts;
  /* NOTE: This is incorrect for files containing groups */
  EXCHECKI(nc_inq(in_exoid, NULL, NULL, &ngatts, NULL));

  /* copy global attributes */
  for (int i = 0; i < ngatts; i++) {

    EXCHECKI(nc_inq_attname(in_exoid, NC_GLOBAL, i, att.name));

    /* if attribute exists in output file, don't overwrite it; compute
     * word size, I/O word size etc. are global attributes stored when
     * file is created with ex_create;  we don't want to overwrite those
     */
    if (nc_inq_att(out_exoid, NC_GLOBAL, att.name, &att.type, &att.len) != EX_NOERR) {

      /* The "last_written_time" attribute is a special attribute used
         by the IOSS library to determine whether a timestep has been
         fully written to the database in order to try to detect a
         database crash that happens in the middle of a database
         output step. Don't want to copy that attribute.
      */
      if (strcmp(att.name, "last_written_time") != 0) {
        /* attribute doesn't exist in new file so OK to create it */
        EXCHECKI(nc_copy_att(in_exoid, NC_GLOBAL, att.name, out_exoid, NC_GLOBAL));
      }
    }
  }

  /* If the `ATT_MAX_NAME_LENGTH` attribute exists on input database,
   * add its value to the output database.  Won't be set above since
   * it is added during ex_create(), so it already exists in the
   * output database, but with potentially wrong value.
   */
  {
    nc_type att_type = NC_NAT;
    size_t  att_len  = 0;
    int     status   = nc_inq_att(in_exoid, NC_GLOBAL, ATT_MAX_NAME_LENGTH, &att_type, &att_len);
    if (status == EX_NOERR) {
      EXCHECKI(nc_copy_att(in_exoid, NC_GLOBAL, ATT_MAX_NAME_LENGTH, out_exoid, NC_GLOBAL));
    }
  }

  return EX_NOERR;
}
/*! \endcond */

/*! \cond INTERNAL */
static int cpy_att(int in_id, int out_id, int var_in_id, int var_out_id)
{
  /* Routine to copy all the attributes from the input netCDF
     file to the output netCDF file. If var_in_id == NC_GLOBAL,
     then the global attributes are copied. Otherwise the variable's
     attributes are copied. */

  int nbr_att;

  if (var_in_id == NC_GLOBAL) {
    EXCHECKI(nc_inq_natts(in_id, &nbr_att));
  }
  else {
    EXCHECKI(nc_inq_varnatts(in_id, var_in_id, &nbr_att));
  }

  /* Get the attributes names, types, lengths, and values */
  for (int idx = 0; idx < nbr_att; idx++) {
    char att_nm[MAX_VAR_NAME_LENGTH];

    EXCHECKI(nc_inq_attname(in_id, var_in_id, idx, att_nm));
    nc_copy_att(in_id, var_in_id, att_nm, out_id, var_out_id);
  }

  return EX_NOERR;
}
/*! \endcond */

/*! \internal */
static int cpy_coord_def(int in_id, int out_id, int rec_dim_id, char *var_nm, int in_large)
{
  /* Handle easiest situation first: in_large matches out_large (1) */
  if (in_large == 1) {
    return cpy_var_def(in_id, out_id, rec_dim_id, var_nm); /* OK */
  }

  /* At this point, know that in_large != out_large, so only supported
     option is that in_large == 0 and out_large == 1.  Also will need
     the spatial dimension, so get that now.
   */
  int         temp;
  size_t      spatial_dim;
  const char *routine = NULL;
  exi_get_dimension(in_id, DIM_NUM_DIM, "dimension", &spatial_dim, &temp, routine);

  /* output file will have coordx, coordy, coordz (if 3d).  See if
     they are already defined in output file. Assume either all or
     none are defined. */

  {
    int var_out_idx, var_out_idy, var_out_idz;
    int status1 = nc_inq_varid(out_id, VAR_COORD_X, &var_out_idx);
    int status2 = nc_inq_varid(out_id, VAR_COORD_Y, &var_out_idy);
    int status3 = nc_inq_varid(out_id, VAR_COORD_Y, &var_out_idz);

    if (status1 == EX_NOERR && status2 == EX_NOERR && (spatial_dim == 2 || status3 == EX_NOERR)) {
      return EX_NOERR; /* already defined in output file */ /* OK */
    }
  }

  /* Get dimid of the num_nodes dimension in output file... */
  int dim_out_id[2];
  EXCHECKI(nc_inq_dimid(out_id, DIM_NUM_NODES, &dim_out_id[0]));

  /* Define the variables in the output file */

  /* Define according to the EXODUS file's IO_word_size */
  int nbr_dim    = 1;
  int var_out_id = -1;
  EXCHECKI(nc_def_var(out_id, VAR_COORD_X, nc_flt_code(out_id), nbr_dim, dim_out_id, &var_out_id));
  exi_compress_variable(out_id, var_out_id, 2);
  if (spatial_dim > 1) {
    EXCHECKI(
        nc_def_var(out_id, VAR_COORD_Y, nc_flt_code(out_id), nbr_dim, dim_out_id, &var_out_id));
    exi_compress_variable(out_id, var_out_id, 2);
  }
  if (spatial_dim > 2) {
    EXCHECKI(
        nc_def_var(out_id, VAR_COORD_Z, nc_flt_code(out_id), nbr_dim, dim_out_id, &var_out_id));
    exi_compress_variable(out_id, var_out_id, 2);
  }

  return var_out_id; /* OK */
}
/*! \endcond */

/*! \internal */
static int cpy_var_def(int in_id, int out_id, int rec_dim_id, char *var_nm)
{
  /* Routine to copy the variable metadata from an input netCDF file
   * to an output netCDF file.
   */

  /* See if the requested variable is already in the output file. */
  int var_out_id;
  int status = nc_inq_varid(out_id, var_nm, &var_out_id);
  if (status == EX_NOERR) {
    return var_out_id; /* OK */
  }

  /* See if the requested variable is in the input file. */
  int var_in_id;
  EXCHECKI(nc_inq_varid(in_id, var_nm, &var_in_id));

  /* Get the type of the variable and the number of dimensions. */
  nc_type var_type;
  EXCHECKI(nc_inq_vartype(in_id, var_in_id, &var_type));
  int nbr_dim;
  EXCHECKI(nc_inq_varndims(in_id, var_in_id, &nbr_dim));

  /* Recall:
     1. The dimensions must be defined before the variable.
     2. The variable must be defined before the attributes. */

  /* Get the dimension IDs */
  int dim_in_id[NC_MAX_VAR_DIMS];
  EXCHECKI(nc_inq_vardimid(in_id, var_in_id, dim_in_id));

  /* Get the dimension sizes and names */
  int dim_out_id[NC_MAX_VAR_DIMS];
  for (int idx = 0; idx < nbr_dim; idx++) {
    char   dim_nm[EX_MAX_NAME + 1];
    size_t dim_sz;

    EXCHECKI(nc_inq_dim(in_id, dim_in_id[idx], dim_nm, &dim_sz));

    /* See if the dimension has already been defined */
    status = nc_inq_dimid(out_id, dim_nm, &dim_out_id[idx]);

    /* If the dimension hasn't been defined, copy it */
    if (status != EX_NOERR) {
      if (dim_in_id[idx] != rec_dim_id) {
        EXCHECKI(nc_def_dim(out_id, dim_nm, dim_sz, &dim_out_id[idx]));
      }
      else {
        EXCHECKI(nc_def_dim(out_id, dim_nm, NC_UNLIMITED, &dim_out_id[idx]));
      }
    }
  }

  /* Define the variable in the output file */

  /* If variable is float or double, define it according to the EXODUS
     file's IO_word_size */

  if ((var_type == NC_FLOAT) || (var_type == NC_DOUBLE)) {
    EXCHECKI(nc_def_var(out_id, var_nm, nc_flt_code(out_id), nbr_dim, dim_out_id, &var_out_id));
    exi_compress_variable(out_id, var_out_id, 2);
  }
  else {
    EXCHECKI(nc_def_var(out_id, var_nm, var_type, nbr_dim, dim_out_id, &var_out_id));
    exi_compress_variable(out_id, var_out_id, 1);
  }
  return var_out_id; /* OK */

} /* end cpy_var_def() */

/*! \internal */
static int cpy_var_val(int in_id, int out_id, char *var_nm)
{
  void *void_ptr = NULL;
  /* Routine to copy the variable data from an input netCDF file
   * to an output netCDF file.
   */

  /* Get the var_id for the requested variable from both files. */
  int var_in_id;
  EXCHECKI(nc_inq_varid(in_id, var_nm, &var_in_id));
  int var_out_id;
  EXCHECKI(nc_inq_varid(out_id, var_nm, &var_out_id));

  /* Get the number of dimensions for the variable. */
  nc_type var_type_out;
  EXCHECKI(nc_inq_vartype(out_id, var_out_id, &var_type_out));

  nc_type var_type_in;
  EXCHECKI(nc_inq_vartype(in_id, var_in_id, &var_type_in));
  int nbr_dim;
  EXCHECKI(nc_inq_varndims(in_id, var_in_id, &nbr_dim));

  /* Get the dimension IDs from the input file */
  int dim_id_in[NC_MAX_VAR_DIMS];
  EXCHECKF(nc_inq_vardimid(in_id, var_in_id, dim_id_in));
  int dim_id_out[NC_MAX_VAR_DIMS];
  EXCHECKF(nc_inq_vardimid(out_id, var_out_id, dim_id_out));

  /* Get the dimension sizes and names from the input file */
  size_t dim_str[NC_MAX_VAR_DIMS];
  size_t dim_cnt[NC_MAX_VAR_DIMS];
  size_t var_sz          = 1L;
  bool   string_len_same = true;

  for (int idx = 0; idx < nbr_dim; idx++) {
    /* NB: For the unlimited dimension, ncdiminq() returns the maximum
       value used so far in writing data for that dimension.
       Thus if you read the dimension sizes from the output file, then
       the ncdiminq() returns dim_sz=0 for the unlimited dimension
       until a variable has been written with that dimension. This is
       the reason for always reading the input file for the dimension
       sizes. */
    size_t dim_in  = 0;
    size_t dim_out = 0;

    /* If client is increasing any sizes, then need to make sure
       the void_ptr is large enough to hold new dimension */
    EXCHECKF(nc_inq_dimlen(in_id, dim_id_in[idx], &dim_in));
    EXCHECKF(nc_inq_dimlen(out_id, dim_id_out[idx], &dim_out));

    /* Initialize the indicial offset and stride arrays */
    size_t dim_max = dim_in > dim_out ? dim_in : dim_out;

    /* Need to know if input and output have different size strings. */
    if (var_type_in == NC_CHAR && idx == 1 && dim_in != dim_out) {
      string_len_same = false;
    }
    var_sz *= dim_max;

    /* Handle case where output variable is smaller than input (rare, but happens) */
    dim_cnt[idx] = dim_out > 0 ? dim_out : dim_in;
    dim_str[idx] = 0;

  } /* end loop over dim */

  /* Allocate enough space to hold the variable */
  if (var_sz > 0) {
    void_ptr = calloc(var_sz, type_size(var_type_in));
  }

  /* Get the variable */

  /* if variable is float or double, convert if necessary */

  if (nbr_dim == 0) { /* variable is a scalar */

    if (var_type_in == NC_INT && var_type_out == NC_INT) {
      EXCHECKF(nc_get_var1_int(in_id, var_in_id, 0L, void_ptr));
      EXCHECKF(nc_put_var1_int(out_id, var_out_id, 0L, void_ptr));
    }

    else if (var_type_in == NC_INT64 && var_type_out == NC_INT64) {
      EXCHECKF(nc_get_var1_longlong(in_id, var_in_id, 0L, void_ptr));
      EXCHECKF(nc_put_var1_longlong(out_id, var_out_id, 0L, void_ptr));
    }

    else if (var_type_in == NC_FLOAT) {
      EXCHECKF(nc_get_var1_float(in_id, var_in_id, 0L, void_ptr));
      EXCHECKF(nc_put_var1_float(out_id, var_out_id, 0L, void_ptr));
    }

    else if (var_type_in == NC_DOUBLE) {
      EXCHECKF(nc_get_var1_double(in_id, var_in_id, 0L, void_ptr));
      EXCHECKF(nc_put_var1_double(out_id, var_out_id, 0L, void_ptr));
    }

    else if (var_type_in == NC_CHAR) {
      EXCHECKF(nc_get_var1_text(in_id, var_in_id, 0L, void_ptr));
      EXCHECKF(nc_put_var1_text(out_id, var_out_id, 0L, void_ptr));
    }

    else {
      assert(1 == 0);
    }
  }
  else { /* variable is a vector */

    if (var_type_in == NC_INT && var_type_out == NC_INT) {
      EXCHECKF(nc_get_var_int(in_id, var_in_id, void_ptr));
      EXCHECKF(nc_put_vara_int(out_id, var_out_id, dim_str, dim_cnt, void_ptr));
    }

    else if (var_type_in == NC_INT64 && var_type_out == NC_INT64) {
      EXCHECKF(nc_get_var_longlong(in_id, var_in_id, void_ptr));
      EXCHECKF(nc_put_vara_longlong(out_id, var_out_id, dim_str, dim_cnt, void_ptr));
    }

    else if (var_type_in == NC_FLOAT) {
      EXCHECKF(nc_get_var_float(in_id, var_in_id, void_ptr));
      EXCHECKF(nc_put_vara_float(out_id, var_out_id, dim_str, dim_cnt, void_ptr));
    }

    else if (var_type_in == NC_DOUBLE) {
      EXCHECKF(nc_get_var_double(in_id, var_in_id, void_ptr));
      EXCHECKF(nc_put_vara_double(out_id, var_out_id, dim_str, dim_cnt, void_ptr));
    }

    else if (var_type_in == NC_CHAR) {

      if (string_len_same) {
        EXCHECKF(nc_get_var_text(in_id, var_in_id, void_ptr));
        EXCHECKF(nc_put_vara_text(out_id, var_out_id, dim_str, dim_cnt, void_ptr));
      }
      else {
        /* Use void_ptr for the input read; alloc new space for output... */
        EXCHECKF(nc_get_var_text(in_id, var_in_id, void_ptr));

        if (void_ptr != NULL) {
          size_t num_string = 0;
          size_t in_size    = 0;
          size_t out_size   = 0;
          EXCHECKF(nc_inq_dimlen(in_id, dim_id_in[0], &num_string));
          EXCHECKF(nc_inq_dimlen(in_id, dim_id_in[1], &in_size));
          EXCHECKF(nc_inq_dimlen(out_id, dim_id_out[1], &out_size));
          size_t min_size    = in_size < out_size ? in_size : out_size;
          char  *out_strings = calloc(num_string * out_size, 1);
          /* Read the input strings...*/

          /* Copy to the output strings...*/
          const char *in_strings = void_ptr;
          for (size_t i = 0; i < num_string; i++) {
            size_t in_off  = i * in_size;
            size_t out_off = i * out_size;
            ex_copy_string(&out_strings[out_off], &in_strings[in_off], min_size);
            out_strings[out_off + out_size - 1] = '\0';
          }
          dim_cnt[1] = out_size;
          EXCHECKF(nc_put_vara_text(out_id, var_out_id, dim_str, dim_cnt, out_strings));
          free(out_strings);
        }
      }
    }

    else {
      assert(1 == 0);
    }
  } /* end if variable is an array */

  /* Free the space that held the variable */
  free(void_ptr);

  return EX_NOERR;

err_ret:
  free(void_ptr);
  return EX_FATAL;

} /* end cpy_var_val() */

/*! \internal */
static int cpy_coord_val(int in_id, int out_id, char *var_nm, int in_large)
{
  /* Routine to copy the coordinate data from an input netCDF file
   * to an output netCDF file.
   */

  /* Handle easiest situation first: in_large matches out_large (1) */
  if (in_large == 1) {
    return cpy_var_val(in_id, out_id, var_nm); /* OK */
  }

  /* At this point, know that in_large == 0, so will need to
     copy a vector to multiple scalars.  Also
     will need a couple dimensions, so get them now.*/
  const char *routine = NULL;
  int         temp;
  size_t      spatial_dim, num_nodes;
  exi_get_dimension(in_id, DIM_NUM_DIM, "dimension", &spatial_dim, &temp, routine);
  exi_get_dimension(in_id, DIM_NUM_NODES, "nodes", &num_nodes, &temp, routine);

  /* output file will have coordx, coordy, coordz (if 3d). */
  /* Get the var_id for the requested variable from both files. */
  int var_in_id, var_out_id[3];
  EXCHECKI(nc_inq_varid(in_id, VAR_COORD, &var_in_id));

  EXCHECKI(nc_inq_varid(out_id, VAR_COORD_X, &var_out_id[0]));
  if (spatial_dim > 1) {
    EXCHECKI(nc_inq_varid(out_id, VAR_COORD_Y, &var_out_id[1]));
  }
  if (spatial_dim > 2) {
    EXCHECKI(nc_inq_varid(out_id, VAR_COORD_Z, &var_out_id[2]));
  }

  nc_type var_type_in, var_type_out;
  EXCHECKI(nc_inq_vartype(in_id, var_in_id, &var_type_in));
  EXCHECKI(nc_inq_vartype(out_id, var_out_id[0], &var_type_out));

  void *void_ptr = NULL;
  if (num_nodes > 0) {
    void_ptr = malloc(num_nodes * type_size(var_type_in));
  }

  /* Copy each component of the variable... */
  size_t start[2], count[2];
  for (size_t i = 0; i < spatial_dim; i++) {
    start[0] = i;
    start[1] = 0;
    count[0] = 1;
    count[1] = num_nodes;
    if (var_type_in == NC_FLOAT) {
      EXCHECKI(nc_get_vara_float(in_id, var_in_id, start, count, void_ptr));
      EXCHECKI(nc_put_var_float(out_id, var_out_id[i], void_ptr));
    }
    else {
      assert(var_type_in == NC_DOUBLE);
      EXCHECKI(nc_get_vara_double(in_id, var_in_id, start, count, void_ptr));
      EXCHECKI(nc_put_var_double(out_id, var_out_id[i], void_ptr));
    }
  }

  /* Free the space that held the variable */
  free(void_ptr);
  return EX_NOERR;

} /* end cpy_coord_val() */

/*! \internal */
static void update_structs(int out_exoid)
{
  update_internal_structs(out_exoid, EX_INQ_EDGE_BLK, exi_get_counter_list(EX_EDGE_BLOCK));
  update_internal_structs(out_exoid, EX_INQ_FACE_BLK, exi_get_counter_list(EX_FACE_BLOCK));
  update_internal_structs(out_exoid, EX_INQ_ELEM_BLK, exi_get_counter_list(EX_ELEM_BLOCK));
  update_internal_structs(out_exoid, EX_INQ_ASSEMBLY, exi_get_counter_list(EX_ASSEMBLY));
  update_internal_structs(out_exoid, EX_INQ_BLOB, exi_get_counter_list(EX_BLOB));

  update_internal_structs(out_exoid, EX_INQ_NODE_SETS, exi_get_counter_list(EX_NODE_SET));
  update_internal_structs(out_exoid, EX_INQ_EDGE_SETS, exi_get_counter_list(EX_EDGE_SET));
  update_internal_structs(out_exoid, EX_INQ_FACE_SETS, exi_get_counter_list(EX_FACE_SET));
  update_internal_structs(out_exoid, EX_INQ_SIDE_SETS, exi_get_counter_list(EX_SIDE_SET));
  update_internal_structs(out_exoid, EX_INQ_ELEM_SETS, exi_get_counter_list(EX_ELEM_SET));

  update_internal_structs(out_exoid, EX_INQ_NODE_MAP, exi_get_counter_list(EX_NODE_MAP));
  update_internal_structs(out_exoid, EX_INQ_EDGE_MAP, exi_get_counter_list(EX_EDGE_MAP));
  update_internal_structs(out_exoid, EX_INQ_FACE_MAP, exi_get_counter_list(EX_FACE_MAP));
  update_internal_structs(out_exoid, EX_INQ_ELEM_MAP, exi_get_counter_list(EX_ELEM_MAP));
}

/*! \internal */
static void update_internal_structs(int out_exoid, ex_inquiry inqcode,
                                    struct exi_list_item **ctr_list)
{
  int64_t number = ex_inquire_int(out_exoid, inqcode);
  if (number > 0) {
    for (int64_t i = 0; i < number; i++) {
      exi_inc_file_item(out_exoid, ctr_list);
    }
  }
}

static size_t type_size(nc_type type)
{
  if (type == NC_CHAR) {
    return sizeof(char); /* OK */
  }
  if (type == NC_INT) {
    return sizeof(int); /* OK */
  }
  if (type == NC_INT64) {
    return sizeof(int64_t); /* OK */
  }
  if (type == NC_FLOAT) {
    return sizeof(float); /* OK */
  }
  if (type == NC_DOUBLE) {
    return sizeof(double); /* OK */
  }
  else {
    return 0; /* OK */
  }
}

/* \endcond */
